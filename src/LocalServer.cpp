#include "system.h"

#include "Log.h"
#include "Message.h"
#include "Server.h"
#include "LocalServer.h"

#define QUEUE_LIMIT 8192

// The LocalServer is a memory queue with file backing for permanence, not
// a full implementation of an on-disk queue.  If this gets too large we
// will eventually run out of memory.

Entry::Entry(char const *fileId, MessageRef message)
{
	this->fileId= fileId;
	this->message= message;
}

Entry::~Entry()
{
}

LocalServer::LocalServer(
	char const *basePath, ServerRef upstream)
{
	this->basePath= basePath;
	this->upstream= upstream;

	assert(this->upstream);

	// Files are names as YYYYMMDD_HHMMSS_NNNN, where the last 4 are
	// just a counter to keep the name unique.

	nameLastTime= (time_t)0;
	nameCounter= 1;
}

LocalServer::~LocalServer()
{
}

// FIXME to test cycling
#define READ_BUFFER 15

bool LocalServer::readFile(char const *path, std::string &data)
{
	bool success= false;

	int fd= open(path, O_RDONLY);
	if (fd == -1) {
		Log::log(LOG_ERROR,
			"Unable to open queue file %s: %s",
			path, strerror(errno));
	} else {
		char buffer[READ_BUFFER + 1];

		bool error= false;
		for (bool run= true; run; ) {
			int bytesRead= read(fd, buffer, READ_BUFFER);
			if (bytesRead == -1) {
				Log::log(LOG_ERROR,
					"Error reading from file %s: %s",
					path, strerror(errno));
				error= true;
			} else if (bytesRead == 0) {
				run= false;
			} else {
				buffer[bytesRead]= '\0';
				data.append(buffer);
			}
		}

		close(fd);
		success= !error;
	}

	return success;
}


bool LocalServer::writeFile(char const *path, char const *data, size_t dataLen)
{
	bool success= false;

	int fd= open(path,
		O_WRONLY|O_CREAT|O_CLOEXEC|O_EXCL,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

	if (fd == -1) {
		Log::log(LOG_ERROR,
			"Unable to open queue file %s: %s",
			path, strerror(errno));
	} else {
		int written= write(fd, data, dataLen);
		if (written == -1) {
			Log::log(LOG_ERROR,
				"Unable to write queue files %s: %s",
				path, strerror(errno));
		} else if ((size_t)written != dataLen) {
			Log::log(LOG_ERROR,
				"Wrong write count from file %s - wanted %d got %d",
				path, dataLen, written);
		} else if (fdatasync(fd) == -1) {
			Log::log(LOG_ERROR,
				"Error in fdatasync on %s: %s",
				path, strerror(errno));
		} else {
			success= true;
		}

		if (close(fd) == -1) {
			Log::log(LOG_ERROR,
				"Error closing out queue file %s: %s",
				path, strerror(errno));

			success= false;
		}

		if (!success) {
			// If we were able to create the file but we were unable to
			// properly write it, try to delete it so we don't introduce
			// bogus data.  If that's not possible because the filesystem
			// is completely sideways - not much we can do.

			if (unlink(path) == -1) {
				Log::log(LOG_ERROR,
					"Unable to unlink faulty file %s: %s",
					path, strerror(errno));
			}
		}
	}

	return success;
}

bool LocalServer::queue(MessageRef message)
{
	bool success= false;

	time_t now;
	time(&now);

	struct tm nowParts;
	if (localtime_r(&now, &nowParts) == NULL) {
		Log::log(LOG_ERROR, "Unable to format time");
		return false;
	}

	int counter= 1;
	{
		std::lock_guard<std::mutex> permit(nameLock);
		if (now != nameLastTime) {
			nameCounter= 1;
			nameLastTime= now;
		}
		counter= nameCounter++;
	}

	char fileId[64];
	sprintf(fileId, "%04d%02d%02d-%02d%02d%02d-%04d",
		nowParts.tm_year + 1900,
		nowParts.tm_mon + 1,
		nowParts.tm_mday,
		nowParts.tm_hour,
		nowParts.tm_min,
		nowParts.tm_sec,
		counter);

	std::string dataPath= basePath;
	dataPath.append("/");
	dataPath.append(fileId);
	dataPath.append(".hl7");

	// We don't attempt to do anything until we're sure the message is
	// flushed out to disk

	success= writeFile(dataPath.c_str(),
		message->getData(), message->getDataLen());

	if (success) {
		Json::Value metaObject= Json::objectValue;
		metaObject["timestamp"]= (int)message->getTimestamp();
		metaObject["remoteHost"]= message->getRemoteHost();

		std::string metaData= Json::FastWriter().write(metaObject);

		std::string metaPath= basePath;
		metaPath.append("/");
		metaPath.append(fileId);
		metaPath.append(".meta");

		success= writeFile(metaPath.c_str(),
			metaData.c_str(), metaData.length());

		if (!success) {
			if (unlink(dataPath.c_str()) == -1) {
				Log::log(LOG_ERROR,
					"Unable to unlink data file %s "
					"after failing to write meta file %s: %s",
					dataPath.c_str(), metaPath.c_str(),
					strerror(errno));
			}
		}
	}

	if (success) {
		EntryRef entry= std::make_shared<Entry>(fileId, message);

		std::lock_guard<std::mutex> permit(writerLock);
		writerQueue.push_back(entry);

		// Wake up the writer and try to push immediately
		writerWake.notify_one();
	}

	return success;
}

bool LocalServer::loadMetadata(
	char const *fileId, time_t &timestamp, std::string &remoteHost)
{
	bool success= false;

	std::string metaPath;
	metaPath.append(basePath);
	metaPath.append(1, '/');
	metaPath.append(fileId);
	metaPath.append(".meta");

	std::string metadata;
	if (!readFile(metaPath.c_str(), metadata)) {
		Log::log(LOG_ERROR,
			"Failed to read metadata file for %s: %s",
			metaPath.c_str(), strerror(errno));	
	} else {
		Json::Reader parser;
		Json::Value data;

		if (!parser.parse(metadata, data, false)) {
			Log::log(LOG_ERROR,
				"Failed to parse JSON metadata file: %s",
				metadata.c_str());
		} else {
			timestamp= (time_t)(data["timestamp"].asInt());
			remoteHost= data["remoteHost"].asString();

			success= true;
		}
	}

	return success;
}

void LocalServer::loadQueueDirectory()
{
	// This routine is called once at startup to load any dangling files
	// from the last run into the memory structure.

	std::list<std::string> fileIdList;

	DIR *dir= opendir(basePath.c_str());
	if (dir == NULL) {
		Log::log(LOG_ERROR,
			"Unable to scan queue directory %s: %s",
			basePath.c_str(), strerror(errno));
	} else {
		struct dirent *de= NULL;
		while ((de= readdir(dir)) != NULL) {
			if (de->d_name[0] != '.') {
				std::string filename(de->d_name);
				size_t offset= filename.rfind('.');
				if (offset != std::string::npos) {
					std::string fileId= filename.substr(0, offset);
					std::string exten= filename.substr(offset + 1);

					if (exten == "hl7") {
						fileIdList.push_back(fileId);
					}
				}
			}
		}
		closedir(dir);
	}

	for (std::string fileId : fileIdList) {
		std::string dataPath;
		dataPath.append(basePath);
		dataPath.append(1, '/');
		dataPath.append(fileId);
		dataPath.append(".hl7");

		std::string data;
		if (readFile(dataPath.c_str(), data)) {
			time_t timestamp;
			std::string remoteHost;

			if (!loadMetadata(fileId.c_str(), timestamp, remoteHost)) {
				timestamp= (time_t)0;
				remoteHost= "LOST";
			}

			Log::log(LOG_DEBUG,
				"Queueing remnant %s from %s at %ul",
				fileId.c_str(), remoteHost.c_str(), timestamp);


			MessageRef message= Message::Create(
				timestamp, remoteHost.c_str(), data.c_str());

			EntryRef entry= Entry::Create(fileId.c_str(), message);

			std::lock_guard<std::mutex> permit(writerLock);
			writerQueue.push_back(entry);
		}
	}
}

#define RETRY_TIMEOUT 20

void LocalServer::writerLoop()
{
	// Load dangling files from a previous run
	loadQueueDirectory();

	while (run) {
		EntryRef entry;
		{
			std::unique_lock<std::mutex> permit(writerLock);
			if (!writerQueue.empty()) {
				entry= writerQueue.front();
				writerQueue.pop_front();
			} else {
				writerWake.wait(permit);

				if (!writerQueue.empty()) {
					entry= writerQueue.front();
					writerQueue.pop_front();
				}
			}
		}

		if (entry) {
			std::string path;
			path.append(basePath);
			path.append("/");
			path.append(entry->getFileId());
			path.append(".hl7");

			if (upstream->queue(entry->getMessage())) {
				// Message was successfully sent, so delete the backing file

				if (unlink(path.c_str()) == -1) {
					Log::log(LOG_ERROR,
						"Unable to clear log file %s: %s",
						path.c_str(), strerror(errno));
				}
			} else {
				Log::log(LOG_WARNING,
					"Unable to send %s - waiting %d seconds to retry",
					entry->getFileId(), RETRY_TIMEOUT);

				{
					std::lock_guard<std::mutex> permit(writerLock);
					writerQueue.push_front(entry);
				}

				sleep(RETRY_TIMEOUT);
			}
		}
	}
}

void LocalServer::start()
{
	run= true;
	writerThread= new std::thread(&LocalServer::writerLoop, this);
}

void LocalServer::stop()
{
	run= false;
	writerWake.notify_one();
	writerThread->join();
	delete writerThread;
}


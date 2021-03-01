#include "system.h"

#include "Log.h"
#include "Server.h"
#include "LocalServer.h"

#define QUEUE_LIMIT 8192

Entry::Entry(char const *filename, char const *message)
{
	this->filename= filename;
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

	nameLastTime= (time_t)0;
	nameCounter= 1;
}

LocalServer::~LocalServer()
{
}

bool LocalServer::queue(char const *data)
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

	char filename[64];
	sprintf(filename, "%04d%02d%02d-%02d%02d%02d-%04d.hl7",
		nowParts.tm_year + 1900,
		nowParts.tm_mon + 1,
		nowParts.tm_mday,
		nowParts.tm_hour,
		nowParts.tm_min,
		nowParts.tm_sec,
		counter);

	std::string path= basePath;
	path.append("/");
	path.append(filename);

	int fd= open(path.c_str(),
		O_WRONLY|O_CREAT|O_CLOEXEC|O_EXCL,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

	if (fd == -1) {
		Log::log(LOG_ERROR,
			"Unable to open queue file %s: %s",
			path.c_str(), strerror(errno));
	} else {
		int size= strlen(data);
		int written= write(fd, data, size);
		if (written == -1) {
			Log::log(LOG_ERROR,
				"Unable to write queue files %s: %s",
				path.c_str(), strerror(errno));
		} else if (written != size) {
			Log::log(LOG_ERROR,
				"Wrong write count from file %s - wanted %d got %d",
				path.c_str(), size, written);
		} else if (fdatasync(fd) == -1) {
			Log::log(LOG_ERROR,
				"Error in fdatasync on %s: %s",
				path.c_str(), strerror(errno));
		} else {
			success= true;
		}

		if (close(fd) == -1) {
			Log::log(LOG_ERROR,
				"Error closing out queue file %s: %s",
				path.c_str(), strerror(errno));

			success= false;
		}

		if (!success) {
			if (unlink(path.c_str()) == -1) {
				Log::log(LOG_ERROR,
					"Unable to unlink faulty file %s: %s",
					path.c_str(), strerror(errno));
			}
		} else {
			EntryRef entry= std::make_shared<Entry>(filename, data);

			std::lock_guard<std::mutex> permit(writerLock);
			writerQueue.push_back(entry);
			writerWake.notify_one();
		}
	}

	return success;
}

// FIXME to test cycling
#define READ_BUFFER 15

void LocalServer::loadQueueDirectory()
{
	std::list<std::string> fileList;

	DIR *dir= opendir(basePath.c_str());
	if (dir == NULL) {
		Log::log(LOG_ERROR,
			"Unable to scan queue directory %s: %s",
			basePath.c_str(), strerror(errno));
	} else {
		struct dirent *de= NULL;
		while ((de= readdir(dir)) != NULL) {
			if (de->d_name[0] != '.') {
				fileList.push_back(de->d_name);
			}
		}
		closedir(dir);
	}

	for (std::string filename : fileList) {
		std::string path;
		path.append(basePath);
		path.append(1, '/');
		path.append(filename);

		int fd= open(path.c_str(), O_RDONLY);
		if (fd == -1) {
			Log::log(LOG_ERROR,
				"Unable to open queue file %s: %s",
				path.c_str(), strerror(errno));
		} else {
			std::string message;
			char buffer[READ_BUFFER + 1];

			bool error= false;
			for (bool run= true; run; ) {
				int bytesRead= read(fd, buffer, READ_BUFFER);
				if (bytesRead == -1) {
					Log::log(LOG_ERROR,
						"Error reading from queue file %s: %s",
						path.c_str(), strerror(errno));
					error= true;
				} else if (bytesRead == 0) {
					run= false;
				} else {
					buffer[bytesRead]= '\0';
					message.append(buffer);
				}
			}

			if (!error && (message.length() > 0)) {
				EntryRef entry= Entry::Create(
					filename.c_str(), message.c_str());

				std::lock_guard<std::mutex> permit(writerLock);
				writerQueue.push_back(entry);
			}

			close(fd);
		}
	}
}

#define RETRY_TIMEOUT 20

void LocalServer::writerLoop()
{
	loadQueueDirectory();

	bool isRetry= false;
	while (run) {
		EntryRef entry;
		{
			std::unique_lock<std::mutex> permit(writerLock);
			if (!writerQueue.empty()) {
				entry= writerQueue.front();
				writerQueue.pop_front();
			} else {
				if (isRetry) {
					writerWake.wait_for(permit,
						std::chrono::seconds(RETRY_TIMEOUT));
				} else {
					writerWake.wait(permit);
				}

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
			path.append(entry->getFilename());

			if (upstream->queue(entry->getMessage())) {
				isRetry= false;

				if (unlink(path.c_str()) == -1) {
					Log::log(LOG_ERROR,
						"Unable to clear log file %s: %s",
						path.c_str(), strerror(errno));
				}
			} else {
				isRetry= true;

				std::lock_guard<std::mutex> permit(writerLock);
				writerQueue.push_front(entry);

				Log::log(LOG_WARNING,
					"Unable to send - repushing %s to front",
					path.c_str());
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


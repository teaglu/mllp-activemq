#include "system.h"

#include "Connection.h"
#include "TcpConnection.h"

#include "Log.h"

TcpConnection::TcpConnection(ListenerRef listener, int sock)
	: Connection(listener)
{
	this->sock= sock;

	stopFlag= false;
	stoppedFlag= true;
}

TcpConnection::~TcpConnection()
{
}

#define READ_BUFFER_SIZE 16

void TcpConnection::readLoop()
{
	for (bool run= true; run; ) {
		{
			std::lock_guard<std::mutex> permit(stopLock);
			run= !stopFlag;
		}

		char buffer[READ_BUFFER_SIZE];

		fd_set readFds;
		FD_ZERO(&readFds);
		FD_SET(sock, &readFds);
		FD_SET(stopPipe[0], &readFds);

		int selectRval= select(
			FD_SETSIZE, &readFds, NULL, NULL, NULL);

		if (selectRval == -1) {
			if (errno != EINTR) {
				Log::log(LOG_ERROR,
					"Error in select waiting for data: %s",
					strerror(errno));

				run= false;
			}
		} else if (selectRval > 0) {
			if (FD_ISSET(sock, &readFds)) {
				int bufferLen= read(sock, buffer, READ_BUFFER_SIZE);

				if (bufferLen < 0) {
					if (errno != EINTR) {
						Log::log(LOG_WARNING,
							"Error in socket read: %s",
							strerror(errno));

						run= false;
					}
				} else if (bufferLen == 0) {
					run= false;
				} else {
					run= handleData(buffer, bufferLen);
					if (!run) {
						Log::log(LOG_WARNING,
							"Connection dropped due to protocol error");
					}
				}
			}
		}

		std::lock_guard<std::mutex> permit(stopLock);
		if (stopFlag) {
			run= false;
		}
	}

	handleEof();

	close(sock);

	// Unregister with listener
	connectionClosed();

	// Notify anyone waiting on the thread
	{
		std::lock_guard<std::mutex> permit(stopLock);
		stoppedFlag= true;
		stopWake.notify_all();
	}

	close(stopPipe[0]);
	close(stopPipe[1]);

	// Derez if this is the last reference
	self= nullptr;
}

bool TcpConnection::write(char const *data, int dataLen)
{
	bool success= false;

	int wrote= ::write(sock, data, dataLen);
	if (wrote < 0) {
		Log::log(LOG_ERROR,
			"Error writing %d bytes on TCP connection: %s",
			dataLen, strerror(errno));
	} else if (wrote != dataLen) {
		Log::log(LOG_ERROR,
			"Underwrite on TCP connection");
	} else {
		success= true;
	}

	return success;
}

void TcpConnection::start()
{
	std::lock_guard<std::mutex> permit(stopLock);
	assert(stoppedFlag);
	stopFlag= false;
	stoppedFlag= false;

	if (pipe(stopPipe) == -1) {
		Log::log(LOG_ERROR,
			"Unable to create UDP stop pipe pair: %s",
			strerror(errno));
	}

	self= shared_from_this();
	this->thread= std::make_shared<std::thread>(
		&TcpConnection::readLoop, this);

	this->thread->detach();
}

void TcpConnection::stop()
{
	{
		std::unique_lock<std::mutex> permit(stopLock);

		if (!stoppedFlag) {
			if (!stopFlag) {
				stopFlag= true;
				if (::write(stopPipe[1], "\0", 1) == -1) {
					Log::log(LOG_ERROR,
						"Error writing to connection stop pipe: %s",
						strerror(errno));
				}
			}

			stopWake.wait(permit);
		}
	}
}


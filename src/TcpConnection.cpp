#include "system.h"

#include "Connection.h"
#include "TcpConnection.h"

#include "Log.h"

TcpConnection::TcpConnection(ListenerRef listener, int sock)
	: Connection(listener)
{
	this->sock= sock;
	this->run= true;
	this->thread= new std::thread(&TcpConnection::readLoop, this);
}

TcpConnection::~TcpConnection()
{
}

#define READ_BUFFER_SIZE 16

void TcpConnection::readLoop()
{
	while (run) {
		char buffer[READ_BUFFER_SIZE];
		int bufferLen= read(sock, buffer, READ_BUFFER_SIZE);

		if (bufferLen < 0) {
			Log::log(LOG_WARNING,
				"Error in socket read: %s",
				strerror(errno));

			run= false;
		} else if (bufferLen == 0) {
			handleEof();
			run= false;
		} else {
			run= handleData(buffer, bufferLen);
			if (!run) {
				Log::log(LOG_WARNING,
					"Connection dropped due to protocol error");
			}
		}
	}

	connectionClosed();
	close(sock);
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

void TcpConnection::stop()
{
	handleKill();
}

void TcpConnection::handleKill()
{
	handleEof();
}

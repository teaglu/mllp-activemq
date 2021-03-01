#include "system.h"

#include "Log.h"
#include "Listener.h"

#include "Connection.h"

#define RECV_BUFFER_SIZE 2047

Listener::Listener(int port)
{
	this->port= port;
}

void Listener::listenLoop()
{
	int sock= socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		Log::log(LOG_ERROR,
			"Unable to create a TCP socket: %s",
			strerror(errno));
	} else {
		struct sockaddr_in addr;
		addr.sin_addr.s_addr= INADDR_ANY;
		addr.sin_family= AF_INET;
		addr.sin_port= htons(port);

		struct sockaddr *genAddr=
			reinterpret_cast<struct sockaddr *>(&addr);

		if (bind(sock, genAddr, sizeof(addr)) == -1) {
			Log::log(LOG_ERROR,
				"Unable to bind TCP port %d: %s",
				port, strerror(errno));
		} else if (listen(sock, 5) == -1) {
			Log::log(LOG_ERROR,
				"Unable to flag socket for listening: %s",
				strerror(errno));
		} else {
			Log::log(LOG_INFO,
				"Listing on TCP port %d", port);

			bool localRun= run;
			while (localRun) {
				fd_set readFds;
				FD_ZERO(&readFds);
				FD_SET(sock, &readFds);
				FD_SET(stopPipe[0], &readFds);

				int selectRval= select(
					FD_SETSIZE, &readFds, NULL, NULL, NULL);

				if (selectRval == -1) {
					Log::log(LOG_ERROR,
						"Error in select waiting for accept: %s",
						strerror(errno));
					sleep(10);
				} else if (selectRval > 0) {
					if (FD_ISSET(sock, &readFds)) {
						socklen_t addrLen= sizeof(addr);
						int clientSock= accept(sock, genAddr, &addrLen);
						if (clientSock == -1) {
							Log::log(LOG_ERROR,
								"Error accepting TCP client: %s",
								strerror(errno));
						} else {
							ConnectionRef connection=
								connect(clientSock, &addr);
							connection->start();
							connectionList.push_back(connection);
						}
					}
				}

				std::lock_guard<std::mutex> guard(runLock);
				localRun= run;
			}

			close(sock);
		}
	}
}

void Listener::connectionClosed(ConnectionRef connection)
{
	std::unique_lock<std::mutex> permit(connectionListLock);
	connectionList.remove(connection);
}

bool Listener::start()
{
	if (pipe(stopPipe) == -1) {
		Log::log(LOG_ERROR,
			"Unable to create UDP stop pipe pair: %s",
			strerror(errno));
	}


	run= true;
	thread= new std::thread(&Listener::listenLoop, this);
	return true;
}

void Listener::stop()
{
	run= false;
	if (write(stopPipe[1], "\0", 1) == -1) {
		Log::log(LOG_ERROR,
			"Error writing to TCP stop pipe: %s",
			strerror(errno));
	}

	thread->join();
	delete thread;
	thread= NULL;

	std::list<ConnectionRef> localList;

	{
		std::unique_lock<std::mutex> permit(connectionListLock);
		for (std::shared_ptr<Connection> conn : connectionList) {
			localList.push_back(conn);
		}
		connectionList.clear();
	}

	for (ConnectionRef conn : localList) {
		Log::log(LOG_INFO, "Force stopping active connection");
		conn->stop();
	}

	close(stopPipe[0]);
	close(stopPipe[1]);
}

Listener::~Listener()
{
}


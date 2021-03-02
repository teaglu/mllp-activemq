#include "system.h"

#include "Log.h"
#include "Listener.h"

#include "Connection.h"

#define RECV_BUFFER_SIZE 2047

Listener::Listener(int family, int port)
{
	this->family= family;
	this->port= port;

	assert((family == AF_INET) || (family == AF_INET6));

	familyName= (family == AF_INET) ? "IP4" : "IP6";
}

void Listener::listenLoop()
{
	int sock= socket(family, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		Log::log(LOG_ERROR,
			"Unable to create an %s TCP socket: %s",
			familyName, strerror(errno));
	} else {
		struct sockaddr_in6 addrBuffer;
		socklen_t addrLen;
		void *addrPart;

		if (family == AF_INET) {
			addrLen= sizeof(struct sockaddr_in);
			struct sockaddr_in *addr=
				reinterpret_cast<struct sockaddr_in *>(&addrBuffer);

			addrPart= &addr->sin_addr;

            addr->sin_family= AF_INET;
			addr->sin_addr.s_addr= INADDR_ANY;
			addr->sin_port= htons(port);
		} else {
			addrLen= sizeof(struct sockaddr_in6);
			struct sockaddr_in6 *addr=
				reinterpret_cast<struct sockaddr_in6 *>(&addrBuffer);

			addrPart= &addr->sin6_addr;

            addr->sin6_family= AF_INET6;
			addr->sin6_addr= in6addr_any;
			addr->sin6_port= htons(port);

			addr->sin6_flowinfo= 0; // Wat?
			addr->sin6_scope_id= 0;

			int bind6Only= 1;
			if (setsockopt(sock,
				IPPROTO_IPV6, IPV6_V6ONLY,
				&bind6Only, sizeof(int)) == -1)
			{
				Log::log(LOG_ERROR,
					"Unable to set IPV6_V6ONLY - bind will probably fail: %s",
					strerror(errno));
			}
		}

		struct sockaddr *addr=
			reinterpret_cast<struct sockaddr *>(&addrBuffer);

		if (bind(sock, addr, addrLen) == -1) {
			Log::log(LOG_ERROR,
				"Unable to bind %s TCP port %d: %s",
				familyName, port, strerror(errno));
		} else if (listen(sock, 5) == -1) {
			Log::log(LOG_ERROR,
				"Unable to flag socket for listening: %s",
				strerror(errno));
		} else {
			Log::log(LOG_INFO,
				"Listening on %s TCP port %d", familyName, port);

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
						int clientSock= accept(sock, addr, &addrLen);
						if (clientSock == -1) {
							Log::log(LOG_ERROR,
								"Error accepting TCP client: %s",
								strerror(errno));
						} else {
							char remoteHost[INET6_ADDRSTRLEN];
							inet_ntop(family,
								addrPart, remoteHost, INET_ADDRSTRLEN);

							ConnectionRef connection=
								connect(clientSock, remoteHost);

							// Push first for no race
							connectionList.push_back(connection);
							connection->start();
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


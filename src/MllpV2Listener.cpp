#include "system.h"

#include "Listener.h"
#include "MllpV2Listener.h"

#include "Connection.h"
#include "TcpConnection.h"
#include "MllpConnection.h"
#include "MllpV2Connection.h"

MllpV2Listener::MllpV2Listener(int port, ServerRef server, char const *queue)
	: Listener(port)
{
	this->server= server;
	this->queue= queue;
}

MllpV2Listener::~MllpV2Listener()
{
}

ConnectionRef MllpV2Listener::connect(int sock, struct sockaddr_in *)
{
	return std::make_shared<MllpV2Connection>(
		shared_from_this(),
		sock,
		server,
		queue.c_str());
}


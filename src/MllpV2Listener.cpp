#include "system.h"

#include "Listener.h"
#include "MllpV2Listener.h"

#include "Connection.h"
#include "TcpConnection.h"
#include "MllpConnection.h"
#include "MllpV2Connection.h"

MllpV2Listener::MllpV2Listener(int family, int port, ServerRef server)
	: Listener(family, port)
{
	this->server= server;
}

MllpV2Listener::~MllpV2Listener()
{
}

ConnectionRef MllpV2Listener::connect(int sock, char const *remoteHost)
{
	return std::make_shared<MllpV2Connection>(
		shared_from_this(),
		sock,
		server,
		remoteHost);
}


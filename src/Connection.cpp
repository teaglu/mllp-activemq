#include "system.h"
#include "Connection.h"

#include "Listener.h"

Connection::Connection(ListenerRef listener)
{
	this->listener= listener;
}

Connection::~Connection()
{
}

void Connection::connectionClosed()
{
	listener->connectionClosed(shared_from_this());
}



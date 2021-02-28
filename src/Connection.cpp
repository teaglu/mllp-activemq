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

void Connection::start()
{
	self= shared_from_this();
}

void Connection::connectionClosed()
{
	listener->connectionClosed(shared_from_this());
}


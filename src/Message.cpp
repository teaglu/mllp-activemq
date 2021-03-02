#include "system.h"

#include "Message.h"

Message::Message(time_t timestamp, char const *remoteHost, char const *data)
{
	this->timestamp= timestamp;
	this->remoteHost= remoteHost;
	this->data= data;
}

Message::~Message()
{
}

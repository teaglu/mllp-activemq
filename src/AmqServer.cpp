#include "system.h"

#include "Log.h"
#include "Server.h"
#include "AmqServer.h"

#define QUEUE_LIMIT 8192

OutboundMessage::OutboundMessage(char const *destination, char const *data)
{
	this->destination= destination;
	this->data= data;
}

AmqServer::AmqServer(
	char const *brokerUri, char const *user, char const *pass)
{
	this->brokerUri= brokerUri;
	this->user= user;
	this->pass= pass;

	factory= 
		new activemq::core::ActiveMQConnectionFactory(brokerUri);

	sendQueue= std::make_shared<std::list<OutboundMessage *>>();
	sendQueueCnt= 0;

	lostDataFd= -1;
	sendQueueOverflow= 0;
}

AmqServer::~AmqServer()
{
	if (!sendQueue->empty()) {
		while (!sendQueue->empty()) {
			OutboundMessage *m= sendQueue->front();
			sendQueue->pop_front();

			saveLostData(m->getDestination(), m->getData());
			delete m;
		}
	}

	if (lostDataFd != -1) {
		close(lostDataFd);
	}

	delete factory;
}

void AmqServer::saveLostData(char const *destination, char const *data)
{
	std::unique_lock<std::mutex> lock(lostDataMutex);

	if (lostDataFd == -1) {
		lostDataFd= open("lost-data.dat",
			O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);

		if (lostDataFd == -1) {
			Log::log(LOG_ERROR,
				"Error opening lost data file: %s",
				strerror(errno));
		}
	}

	if (lostDataFd != -1) {
		char delim[]= "\n\n";
		int delimLen= strlen(delim);

		struct iovec parts[5];
		parts[0].iov_base= (void *)destination;
		parts[0].iov_len= strlen(destination);
		parts[1].iov_base= delim;
		parts[1].iov_len= delimLen;
		parts[2].iov_base= (void *)data;
		parts[2].iov_len= strlen(data);
		parts[3].iov_base= delim;
		parts[3].iov_len= delimLen;

		if (writev(lostDataFd, parts, 4) == -1) {
			Log::log(LOG_ERROR,
				"Error writing lost data: %s",
				strerror(errno));
		}
	}
}

void AmqServer::onException(const cms::CMSException &ex)
{
	std::string err= ex.getMessage();

	Log::log(LOG_ERROR,
		"CMS Exception on ExceptionListener: %s",
		err.c_str());

	error= true;
	sendQueueCond.notify_one();
}

bool AmqServer::queue(char const *destination, char const *data)
{
	std::unique_lock<std::mutex> lock(sendQueueMutex);

	if (sendQueueCnt >= QUEUE_LIMIT) {
		if (sendQueueOverflow == 0) {
			Log::log(LOG_WARNING,
				"Writing data to lost file at %d messages",
				sendQueueCnt);
		}
		saveLostData(destination, data);	
		sendQueueOverflow++;
	} else {
		OutboundMessage *m= new OutboundMessage(destination, data);

		sendQueue->push_back(m);
		sendQueueCnt++;

		sendQueueCond.notify_one();
	}

	return true;
}

bool AmqServer::connect()
{
	bool rval= false;

	try {
		connection= factory->createConnection(user, pass);

		// Create the session for pushing messages
		session= connection->createSession(
			cms::Session::CLIENT_ACKNOWLEDGE);

		connection->setExceptionListener(this);

		connection->start();

		Log::log(LOG_INFO,
			"Connected to MQ server");

		rval= true;
	} catch (const cms::CMSException& ex) {
		auto err= ex.getMessage();

		Log::log(LOG_ERROR,
			"CMS Exception connecting: %s",
			err.c_str());
	}

	return rval;
}

void AmqServer::disconnect()
{
	try {
		if (connection != NULL) {
			connection->close();
		}
	} catch (const cms::CMSException& ex2) {
		auto err= ex2.getMessage();

		Log::log(LOG_ERROR,
				"Error closing connection: %s",
				err.c_str());
	}

	if (session != NULL) {
		delete session;
		session= NULL;
	}
	if (connection != NULL) {
		delete connection;
		connection= NULL;
	}
}

bool AmqServer::send(OutboundMessage *msg)
{
	bool rval= false;

	cms::Destination *destination= NULL;
	cms::MessageProducer *producer= NULL;
	cms::TextMessage *message= NULL;

	try {
		destination= session->createQueue(msg->getDestination());
		producer= session->createProducer(destination);
		message= session->createTextMessage(msg->getData());
		producer->send(message);

		rval= true;
	} catch (const cms::CMSException &e) {
		std::string error= e.getMessage();

		Log::log(LOG_ERROR,
			"Error sending sendQueued message: %s",
			error.c_str());
	}

	if (message != NULL) {
		delete message;
	}
	if (producer != NULL) {
		delete producer;
	}
	if (destination != NULL) {
		delete destination;
	}

	return rval;
}

void AmqServer::runLoop()
{
	while (run) {
		error= false;

		if (connect()) {
			while (run && !error) {
				std::shared_ptr<std::list<OutboundMessage *>> localQueue;

				// We don't want to hang calling threads during any errors
				// that we may have, so the idea is to pull all the items
				// onto a local list.  The easier way is to just take the
				// entire list and leave a new on in it's place.
				//
				// If there are errors we have to push them back on the
				// front of the list, but this way we're optimizing for
				// the success case.
				{
					std::unique_lock<std::mutex> lock(sendQueueMutex);
					sendQueueCond.wait(lock);
					if (!sendQueue->empty()) {
						localQueue= sendQueue;

						sendQueue= std::make_shared<
							std::list<OutboundMessage *>>();
						sendQueueCnt= 0;
					}
				}

				if (localQueue) {
					while (run && !error && !localQueue->empty()) {
						OutboundMessage *m= localQueue->front();

						if (send(m)) {
							localQueue->pop_front();
							delete m;
						} else {
							error= true;
						}
					}

					std::unique_lock<std::mutex> lock(sendQueueMutex);
					if (!localQueue->empty()) {
						// Push any failures back on the front
						while (!localQueue->empty()) {
							OutboundMessage *m= localQueue->back();
							localQueue->pop_back();

							sendQueue->push_front(m);
							sendQueueCnt++;
						}
					} else {
						if (sendQueueOverflow > 0) {
							Log::log(LOG_INFO,
								"Lost Data: %d messages in lost data file",
								sendQueueOverflow);
						}
					}
				}
			}
		}

		Log::log(LOG_INFO, "Tearing down connection");
		disconnect();

		if (run) {
			sleep(5);
		}
	}
}

void AmqServer::start()
{
	run= true;
	thread= new std::thread(&AmqServer::runLoop, this);
}

void AmqServer::stop()
{
	run= false;
	sendQueueCond.notify_one();
	thread->join();
	delete thread;
}


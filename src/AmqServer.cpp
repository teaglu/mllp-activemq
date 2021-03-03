#include "system.h"

#include "Log.h"
#include "Message.h"
#include "Server.h"
#include "AmqServer.h"

#define QUEUE_LIMIT 8192

Frame::Frame(MessageRef message)
{
	this->message= message;
	this->success= false;
	this->abandoned= false;
}

bool Frame::await(int timeout) {
	std::unique_lock<std::mutex> lock(completeLock);

	if (completeWake.wait_for(lock,
		std::chrono::seconds(timeout)) == std::cv_status::timeout)
	{
		Log::log(LOG_WARNING, "Timeout waiting for call frame");

		abandoned= true;
	}

	return success;
}

void Frame::complete(bool success) {
	this->success= success;
	completeWake.notify_one();
}

AmqServer::AmqServer(
	char const *brokerUri, char const *user, char const *pass,
	char const *queueName,
	bool jsonEnvelope)
{
	this->brokerUri= brokerUri;
	this->user= user;
	this->pass= pass;
	this->queueName= queueName;
	this->jsonEnvelope= jsonEnvelope;

	factory= 
		new activemq::core::ActiveMQConnectionFactory(brokerUri);

	connection= NULL;
	session= NULL;
}

AmqServer::~AmqServer()
{
	sendQueue.clear();
	delete factory;
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

bool AmqServer::queue(MessageRef message)
{
	FrameRef frame= std::make_shared<Frame>(message);

	{
		std::lock_guard<std::mutex> lock(sendQueueLock);
		sendQueue.push_back(frame);
	}

	sendQueueCond.notify_one();

	return frame->await(10);
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

bool AmqServer::send(FrameRef frame)
{
	bool rval= false;

	if (frame->isAbandoned()) {
		Log::log(LOG_INFO,
			"Ignoring abandoned frame");

		return true;
	}

	cms::Destination *destination= NULL;
	cms::MessageProducer *producer= NULL;
	cms::TextMessage *message= NULL;

	try {
		destination= session->createQueue(queueName.c_str());
		producer= session->createProducer(destination);

		long long javaTimestamp=
			frame->getMessage()->getTimestamp() * 1000L;

		if (jsonEnvelope) {
			Json::Value envelope= Json::objectValue;
			envelope["message"]= frame->getMessage()->getData();
			envelope["timestamp"]=  javaTimestamp;
			envelope["remoteHost"]= frame->getMessage()->getRemoteHost();

			std::string bodyString= Json::FastWriter().write(envelope);
			message= session->createTextMessage(bodyString.c_str());
		} else {
			message= session->createTextMessage(
				frame->getMessage()->getData());
		}

		message->setLongProperty("MLLP-Timestamp", javaTimestamp);
		message->setStringProperty("MLLP-RemoteHost",
			frame->getMessage()->getRemoteHost());

		producer->send(message);

		rval= true;
	} catch (const cms::CMSException &e) {
		std::string error= e.getMessage();

		Log::log(LOG_ERROR,
			"Error sending message: %s",
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
				FrameRef frame= nullptr;
				{
					std::unique_lock<std::mutex> lock(sendQueueLock);
					if (!sendQueue.empty()) {
						frame= sendQueue.front();
						sendQueue.pop_front();
					} else {
						sendQueueCond.wait(lock);
						if (!sendQueue.empty()) {
							frame= sendQueue.front();
							sendQueue.pop_front();
						}
					}
				}

				if (frame) {
					bool success= send(frame);
					if (!success) {
						error= true;
					}
					frame->complete(success);
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


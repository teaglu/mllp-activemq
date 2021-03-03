class Message;
typedef std::shared_ptr<Message> MessageRef;

class Frame {
private:
	MessageRef message;

	std::mutex completeLock;
	std::condition_variable completeWake;

	bool abandoned;
	bool success;

public:
	Frame(MessageRef message);

	MessageRef getMessage() {
		return message;
	}

	bool isAbandoned() {
		std::lock_guard<std::mutex> permit(completeLock);
		return abandoned;
	}

	bool await(int timeout);
	void complete(bool success);
};

typedef std::shared_ptr<Frame> FrameRef;

class Server;
class AmqServer : public Server, public cms::ExceptionListener {
private:
	std::string brokerUri;
	std::string user;
	std::string pass;
	std::string queueName;
	bool jsonEnvelope;

	cms::ConnectionFactory *factory;
	cms::Connection *connection;
	cms::Session *session;

	std::thread *thread;

	std::list<FrameRef> sendQueue;
	std::mutex sendQueueLock;
	std::condition_variable sendQueueCond;

	virtual void onException(const cms::CMSException &ex);

	volatile bool run;
	volatile bool error;

protected:
	bool connect();
	void disconnect();
	bool send(FrameRef);

	void runLoop();

public:
	AmqServer(
		char const *brokerURI,
		char const *user,
		char const *pass,
		char const *queueName,
		bool jsonEnvelope);

	static ServerRef Create(
        char const *uri,
        char const *user,
        char const *pass,
		char const *queueName,
		bool jsonEnvelope)
	{
		return std::make_shared<AmqServer>(
			uri, user, pass, queueName, jsonEnvelope);
	}

	virtual ~AmqServer();

	bool queue(MessageRef);

	void start();
	void stop();
};


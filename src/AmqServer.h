class Frame {
private:
	std::string data;

	std::mutex completeLock;
	std::condition_variable completeWake;

	bool success;

public:
	Frame(char const *data);

	char const *getData() {
		return data.c_str();
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
		char const *queueName);

	static ServerRef Create(
        char const *uri,
        char const *user,
        char const *pass,
		char const *queueName)
	{
		return std::make_shared<AmqServer>(uri, user, pass, queueName);
	}

	virtual ~AmqServer();

	bool queue(char const *data);

	void start();
	void stop();
};


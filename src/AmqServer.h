class OutboundMessage {
private:
	std::string destination;
	std::string data;

public:
	OutboundMessage(char const *dest, char const *data);

	char const *getDestination() {
		return destination.c_str();
	}

	char const *getData() {
		return data.c_str();
	}
};

class Server;
class AmqServer : public Server, public cms::ExceptionListener {
private:
	std::string brokerUri;
	std::string user;
	std::string pass;

	cms::ConnectionFactory *factory;
	cms::Connection *connection;
	cms::Session *session;

	std::thread *thread;

	std::shared_ptr<std::list<OutboundMessage *>> sendQueue;
	int sendQueueCnt;
	std::mutex sendQueueMutex;
	std::condition_variable sendQueueCond;
	int sendQueueOverflow;

	std::mutex lostDataMutex;
	int lostDataFd;

	virtual void onException(const cms::CMSException &ex);

	volatile bool run;
	volatile bool error;

protected:
	bool connect();
	void disconnect();
	bool send(OutboundMessage *);

	void saveLostData(char const *destination, char const *data);

	void runLoop();

public:
	AmqServer(
		char const *brokerURI,
		char const *user,
		char const *pass);

	static ServerRef Create(
        char const *uri,
        char const *user,
        char const *pass)
	{
		return std::make_shared<AmqServer>(uri, user, pass);
	}

	virtual ~AmqServer();

	bool queue(char const *destination, char const *data);

	void start();
	void stop();
};


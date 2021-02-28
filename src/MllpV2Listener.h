class Connection;
typedef std::shared_ptr<Connection> ConnectionRef;

class Server;
typedef std::shared_ptr<Server> ServerRef;

class MllpV2Listener : public Listener {
public:
	MllpV2Listener(int port, ServerRef server, char const *queue);
	virtual ~MllpV2Listener();

	static ListenerRef Create(
		int port,
		ServerRef server,
		char const *queue)
	{
		return std::make_shared<MllpV2Listener>(port, server, queue);
	}

protected:
	virtual ConnectionRef connect(int sock, struct sockaddr_in *);

private:
	ServerRef server;
	std::string queue;
};


class Connection;
typedef std::shared_ptr<Connection> ConnectionRef;

class Server;
typedef std::shared_ptr<Server> ServerRef;

class MllpV2Listener : public Listener {
public:
	MllpV2Listener(int family, int port, ServerRef server);
	virtual ~MllpV2Listener();

	static ListenerRef Create(
		int family,
		int port,
		ServerRef server)
	{
		return std::make_shared<MllpV2Listener>(family, port, server);
	}

protected:
	virtual ConnectionRef connect(int sock, char const *remoteHost);

private:
	ServerRef server;
};


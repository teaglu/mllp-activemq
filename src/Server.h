class Server {
public:
	Server();
	virtual ~Server();

	virtual bool queue(char const *destination, char const *data) = 0;

	virtual void start() = 0;
	virtual void stop() = 0;
};

typedef std::shared_ptr<Server> ServerRef;

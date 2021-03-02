class Message;
typedef std::shared_ptr<Message> MessageRef;

class Server {
public:
	Server();
	virtual ~Server();

	virtual bool queue(MessageRef) = 0;

	virtual void start() = 0;
	virtual void stop() = 0;
};

typedef std::shared_ptr<Server> ServerRef;


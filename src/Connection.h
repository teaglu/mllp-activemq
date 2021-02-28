class Listener;
typedef std::shared_ptr<Listener> ListenerRef;

class Connection
	: public std::enable_shared_from_this<Connection>
{
public:
	Connection(ListenerRef listener);
	virtual ~Connection();

	virtual void start();
	virtual void stop() = 0;

private:
	std::shared_ptr<Connection> self;
	ListenerRef listener;

protected:
	virtual void connectionClosed();
};


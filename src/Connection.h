class Listener;
typedef std::shared_ptr<Listener> ListenerRef;

class Connection
	: public std::enable_shared_from_this<Connection>
{
public:
	Connection(ListenerRef listener);
	virtual ~Connection();

	virtual void start() = 0;
	virtual void stop() = 0;

private:
	ListenerRef listener;

protected:
	virtual void connectionClosed();
};


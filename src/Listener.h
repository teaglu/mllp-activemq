
class Connection;
typedef std::shared_ptr<Connection> ConnectionRef;

class Listener
	: public std::enable_shared_from_this<Listener>
{
protected:
	virtual ConnectionRef connect(int sock, char const *remoteHost) = 0;

private:
    std::thread *thread;

    int stopPipe[2];

    volatile bool run;
	std::mutex runLock;

	int port;
	int family;

	char const *familyName;

    void listenLoop();

	std::mutex connectionListLock;
	std::list<std::shared_ptr<Connection>> connectionList;

public:
	Listener(int family, int port);
	virtual ~Listener();

	virtual bool start();
	virtual void stop();

	void connectionClosed(std::shared_ptr<Connection>);
};

typedef std::shared_ptr<Listener> ListenerRef;


class TcpConnection
	: public Connection
{
public:
	TcpConnection(ListenerRef listener, int sock);
	virtual ~TcpConnection();

	virtual void start() override;
	virtual void stop() override;

private:
	int sock;

	std::shared_ptr<std::thread> thread;
	std::shared_ptr<Connection> self;

	std::mutex stopLock;
	bool stopFlag;
	bool stoppedFlag;
	std::condition_variable stopWake;

	int stopPipe[2];

	std::condition_variable stopWait;

	void readLoop();

protected:
	virtual bool handleData(char const *data, int dataLen) = 0;
	virtual void handleEof() = 0;

	virtual bool write(char const *data, int dataLen);
};


class TcpConnection
	: public Connection
{
public:
	TcpConnection(ListenerRef listener, int sock);
	virtual ~TcpConnection();

	virtual void stop();

private:
	int sock;
	std::thread *thread;
	volatile bool run;

	void readLoop();

protected:
	virtual bool handleData(char const *data, int dataLen) = 0;
	virtual void handleKill();
	virtual void handleEof() = 0;

	virtual bool write(char const *data, int dataLen);
};


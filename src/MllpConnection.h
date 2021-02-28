class Server;
typedef std::shared_ptr<Server> ServerRef;

class MllpConnection
	: public TcpConnection
{
public:
	MllpConnection(
		ListenerRef listener,
		int sock,
		ServerRef server,
		char const *queue);

	virtual ~MllpConnection();

	enum class AckType {
		ACCEPT,
		ERROR,
		REJECT
	};

protected:
	virtual bool handleData(char const *data, int dataLen);
	virtual void handleEof();

protected:
	virtual bool parse(char const *message) = 0;
	virtual void acknowledge(AckType) = 0;

private:
	ServerRef server;
	std::string queue;

	enum class MllpState {
		WAIT_SB,
		READ_MESSAGE,
		WAIT_CR
	};

	MllpState mllpState;
	std::string mllpMessage;

	bool handleMessage(char const *message);
};


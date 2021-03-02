class MllpV2Connection
	: public MllpConnection
{
public:
	MllpV2Connection(
		ListenerRef listener,
		int sock,
		ServerRef server,
		char const *remoteHost);

	virtual ~MllpV2Connection();

protected:
	virtual bool parse(char const *message);
	virtual void acknowledge(AckType);

private:
	void sendResponse(bool success);

	std::string fromApp;
	std::string fromFacility;
	std::string toApp;
	std::string toFacility;
	std::string messageId;
};


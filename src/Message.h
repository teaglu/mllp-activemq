class Message {
public:
	Message(
		time_t timestamp,
		char const *remoteHost,
		char const *data);

	static std::shared_ptr<Message> Create(
		time_t timestamp,
		char const *remoteHost,
		char const *data)
	{
		return std::make_shared<Message>(timestamp, remoteHost, data);
	}


	virtual ~Message();

	char const *getData() {
		return data.c_str();
	}
	size_t getDataLen() {
		return data.length();
	}

	time_t getTimestamp() {
		return timestamp;
	}

	char const *getRemoteHost() {
		return remoteHost.c_str();
	}

private:
	time_t timestamp;
	std::string remoteHost;
	std::string data;
};

typedef std::shared_ptr<Message> MessageRef;

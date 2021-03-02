class Server;
typedef std::shared_ptr<Server> ServerRef;

class Entry {
public:
	Entry(char const *fileId, MessageRef message);

	static std::shared_ptr<Entry> Create(
		char const *fileId, MessageRef message)
	{
		return std::make_shared<Entry>(fileId, message);
	}

	virtual ~Entry();

	char const *getFileId() {
		return fileId.c_str();
	}
	MessageRef getMessage() {
		return message;
	}

private:
	std::string fileId;
	MessageRef message;
};

typedef std::shared_ptr<Entry> EntryRef;

class LocalServer : public Server {
private:
	std::string basePath;
	std::thread *writerThread;

	ServerRef upstream;

	std::mutex nameLock;
	int nameCounter;
	time_t nameLastTime;

	std::deque<EntryRef> writerQueue;
	std::mutex writerLock;
	std::condition_variable writerWake;

	bool loadMetadata(
		char const *fileId, time_t &timestamp, std::string &remoteHost);

	void loadQueueDirectory();

	volatile bool run;

	bool readFile(char const *path, std::string &data);
	bool writeFile(char const *path, char const *data, size_t dataLen);

protected:
	void writerLoop();

public:
	LocalServer(char const *, ServerRef);

	static ServerRef Create(char const *basePath, ServerRef upstream)
	{
		return std::make_shared<LocalServer>(basePath, upstream);
	}

	virtual ~LocalServer();

	virtual bool queue(MessageRef) override;

	virtual void start() override;
	virtual void stop() override;
};


class Server;
typedef std::shared_ptr<Server> ServerRef;

class Entry {
public:
	Entry(char const *filename, char const *message);

	static std::shared_ptr<Entry> Create(
		char const *filename, char const *message)
	{
		return std::make_shared<Entry>(filename, message);
	}

	virtual ~Entry();

	char const *getFilename() {
		return filename.c_str();
	}
	char const *getMessage() {
		return message.c_str();
	}

private:
	std::string filename;
	std::string message;
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

	void loadQueueDirectory();

	volatile bool run;

protected:
	void writerLoop();

public:
	LocalServer(char const *, ServerRef);

	static ServerRef Create(char const *basePath, ServerRef upstream)
	{
		return std::make_shared<LocalServer>(basePath, upstream);
	}

	virtual ~LocalServer();

	virtual bool queue(char const *data) override;

	virtual void start() override;
	virtual void stop() override;
};


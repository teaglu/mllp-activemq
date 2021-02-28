#define LOG_DEBUG		0
#define LOG_INFO		1
#define LOG_WARNING		2
#define LOG_ERROR		3
#define LOG_CRITICAL	4

class Log {
public:
	static void log(int, char const *, ...);

	static void open(char const *);
	static void setLogLevel(int);

private:
	static pthread_mutex_t lock;
	static int fd;
	static int logLevel;
};


#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include <activemq/library/ActiveMQCPP.h>
#include <decaf/lang/Thread.h>
#include <decaf/lang/Runnable.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <decaf/lang/Integer.h>
#include <decaf/lang/Long.h>
#include <decaf/lang/System.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/util/Config.h>
#include <cms/Connection.h>
#include <cms/Session.h>
#include <cms/TextMessage.h>
#include <cms/BytesMessage.h>
#include <cms/MapMessage.h>
#include <cms/ExceptionListener.h>
#include <cms/MessageListener.h>

#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <list>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <math.h>

#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG

#include <assert.h>

AM_CXXFLAGS = -g -std=c++11 -pthread -Wall -fno-strict-aliasing \
	-Wno-deprecated-declarations \
	-I/usr/include/activemq-cpp-3.9.3 \
	-I/usr/local/include/activemq-cpp-3.9.5 \
	-I/usr/include/apr-1 \
	-I/usr/local/include/apr-1 \
	-I/usr/include/jsoncpp

bin_PROGRAMS = mllp-activemq

mllp_activemq_SOURCES = \
	Message.cpp \
	Server.cpp \
	AmqServer.cpp \
	LocalServer.cpp \
	Listener.cpp \
	Connection.cpp \
	TcpConnection.cpp \
	MllpConnection.cpp \
	MllpV2Connection.cpp \
	MllpV2Listener.cpp \
	Log.cpp \
	DateUtil.cpp \
	main.cpp

mllp_activemq_LDFLAGS = -pthread
mllp_activemq_LDADD = -lactivemq-cpp -ljsoncpp -lcrypto


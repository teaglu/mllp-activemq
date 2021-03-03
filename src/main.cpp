#include "system.h"

#include "Server.h"
#include "AmqServer.h"
#include "LocalServer.h"

#include "Listener.h"
#include "MllpV2Listener.h"

#include "Log.h"

static volatile bool rundown;

static void doExit(int junk)
{
	Log::log(LOG_INFO, "Queueing exit because of signal");
	rundown= true;
}

int main(int argc, char* argv[])
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, doExit);
	signal(SIGINT, doExit);

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	activemq::library::ActiveMQCPP::initializeLibrary();

	// The activemq verifier doesn't trust wildcards for whatever
	// reason, so this is necessary to connect to SSL.  This needs
	// to be fixed for security.

	decaf::lang::System::setProperty(
		"decaf.net.ssl.disablePeerVerification", "true");

	int mllpVersion= 2;
	int mllpPort= 2575;

	char const *brokerUri= getenv("AMQ_BROKER_URI");
	char const *brokerUser= getenv("AMQ_BROKER_USER");
	char const *brokerPass= getenv("AMQ_BROKER_PASS");
	char const *queueName= getenv("QUEUE");
	char const *localQueuePath= getenv("LOCALQUEUE_PATH");

	bool jsonEnvelope= false;

	int c;
	while ((c= getopt(argc, argv, "S:U:P:Q:L:j")) != -1) {
		switch (c) {
		case 'p':
			mllpPort= atoi(optarg);
			if ((mllpPort < 1024) || (mllpPort > 65535)) {
				Log::log(LOG_ERROR,
					"MLLP port is invalid");
				exit(1);
			}
			break;

		case 'S':
			brokerUri= optarg;
			break;

		case 'U':
			brokerUser= optarg;
			break;

		case 'P':
			brokerPass= optarg;
			break;

		case 'Q':
			queueName= optarg;
			break;

		case 'L':
			localQueuePath= optarg;
			break;

		case 'j':
			jsonEnvelope= true;
			break;

		default:
			fprintf(stderr, "Unknown argument %c\n", optopt);
			exit(1);
		}
	}

	bool valid= true;

	if (brokerUri == NULL) {
		Log::log(LOG_CRITICAL, "Broker URI not specified");
		valid= false;
	}
	if (brokerUser == NULL) {
		Log::log(LOG_CRITICAL, "Broker user not specified");
		valid= false;
	}

	if (valid) {
   		ServerRef amqServer= AmqServer::Create(
			brokerUri, brokerUser, brokerPass, queueName, jsonEnvelope);
		amqServer->start();

		ServerRef server= amqServer;

		ServerRef localServer;
		if (localQueuePath != NULL) {
			localServer= LocalServer::Create(
				localQueuePath, amqServer);

			localServer->start();

			server= localServer;
		}

		ListenerRef ip4Listener=
			MllpV2Listener::Create(AF_INET, mllpPort, server);

		ListenerRef ip6Listener=
			MllpV2Listener::Create(AF_INET6, mllpPort, server);

		ip4Listener->start();
		ip6Listener->start();

		for (rundown= false; !rundown; ) {
			pause();
		}

		Log::log(LOG_INFO, "Stopping Listener");
		ip4Listener->stop();
		ip6Listener->stop();

		if (localServer) {
			Log::log(LOG_INFO, "Stopping local queue");
			localServer->stop();
		}

		Log::log(LOG_INFO, "Stopping MQ Connection");
		amqServer->stop();
	}

	activemq::library::ActiveMQCPP::shutdownLibrary();

	// Cleanup all the OpenSSL stuff
	CONF_modules_unload(1);
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

	// This makes openssl dump core if activemq had any SSL connections.
	// I was trying to make a clean shutdown so valgrid output would be
	// useful, but we don't actually need to free this since we're exiting
	// anyway. -DAW 190310

	//ERR_remove_state(0);

	ERR_free_strings();

	Log::log(LOG_INFO, "Normal shutdown");
}


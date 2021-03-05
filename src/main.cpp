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

	int mllpVersion= 2;
	int mllpPort= 2575;

	char const *brokerUri= getenv("AMQ_URI");
	char const *brokerUser= getenv("AMQ_USERNAME");
	char const *brokerPass= getenv("AMQ_PASSWORD");
	char const *queueName= getenv("AMQ_QUEUE");
	char const *localQueuePath= getenv("LOCALQUEUE_PATH");

	bool jsonEnvelope= false;
	bool peerValidation= true;

	int c;
	while ((c= getopt(argc, argv, "S:U:P:Q:L:p:ji")) != -1) {
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

		case 'i':
			peerValidation= false;
			break;

		default:
			fprintf(stderr, "Unknown argument %c\n", optopt);
			exit(1);
		}
	}

	if (brokerUri == NULL) {
		Log::log(LOG_CRITICAL, "Broker URI not specified");
		exit(1);
	}
	if (brokerUser == NULL) {
		Log::log(LOG_CRITICAL, "Broker user not specified");
		exit(1);
	}

	activemq::library::ActiveMQCPP::initializeLibrary();

	if (!peerValidation) {
		// This is sometimes necessary because the activemq verifier
		// doesn't trust wildcard certificates.  Until they fix that
		// bug you have to disable peer validation to get that to work.

		Log::log(LOG_WARNING, "Disabling SSL Peer Validation");

		decaf::lang::System::setProperty(
			"decaf.net.ssl.disablePeerVerification", "true");
	}

	// Block so everything gets de-rezzed before we shut the libs down
	{
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


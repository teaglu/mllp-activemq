#include "system.h"

#include "Connection.h"
#include "TcpConnection.h"
#include "MllpConnection.h"

#include "Message.h"
#include "Server.h"

#include "Log.h"

MllpConnection::MllpConnection(
	ListenerRef listener,
	int sock,
	ServerRef server,
	char const *remoteHost)
	: TcpConnection(listener, sock)
{
	this->server= server;
	this->remoteHost= remoteHost;

	mllpState= MllpState::WAIT_SB;
}

MllpConnection::~MllpConnection()
{
}

bool MllpConnection::handleData(char const *data, int dataLen)
{
	bool valid= true;
	for (int i= 0; valid && (i < dataLen); i++) {
		char c= data[i];

		switch (mllpState) {
		case MllpState::WAIT_SB:
			if (c != 0x0B) {
				Log::log(LOG_ERROR,
					"Char received other than SB");
				valid= false;
			} else {
				mllpState= MllpState::READ_MESSAGE;
			}
			break;

		case MllpState::READ_MESSAGE:
			if (c == 0x1C) {
				mllpState= MllpState::WAIT_CR;
			} else if ((c == 0x0D) || ((c > 0x1F) && (c <= 0x7F))) {
				mllpMessage.append(1, c);
			} else {
				Log::log(LOG_ERROR,
					"Invalid character %02X received in message",
					c);
				valid= false;
			}
			break;

		case MllpState::WAIT_CR:
			if (c == 0x0D) {
				if (handleMessage(mllpMessage.c_str())) {
					mllpState= MllpState::WAIT_SB;
				} else {
					Log::log(LOG_ERROR,
						"Failed to process MLLP message");
					valid= false;
				}
			} else {
				Log::log(LOG_ERROR,
					"Expected CR after EB missing");

				valid= false;
			}
			break;
		}
	}

	return valid;
}

void MllpConnection::handleEof()
{
}

bool MllpConnection::handleMessage(char const *data)
{
	bool success= false;

	if (parse(data)) {
		time_t now;
		time(&now);

		MessageRef message= Message::Create(now, remoteHost.c_str(), data);
		if (server->queue(message)) {
			acknowledge(AckType::ACCEPT);
			success= true;
		} else {
			acknowledge(AckType::ERROR);
		}
	} else {
		acknowledge(AckType::REJECT);
	}

	return success;
}


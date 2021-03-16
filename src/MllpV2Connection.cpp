#include "system.h"

#include "Connection.h"
#include "TcpConnection.h"
#include "MllpConnection.h"
#include "MllpV2Connection.h"

#include "Log.h"

MllpV2Connection::MllpV2Connection(
	ListenerRef listener,
	int sock,
	ServerRef server,
	char const *remoteHost)
	: MllpConnection(listener, sock, server, remoteHost)
{
}

MllpV2Connection::~MllpV2Connection()
{
}

void MllpV2Connection::split(
	std::string& line,
	char separator,
	std::vector<std::string>& parts)
{
	size_t start= 0;
	for (size_t end= 0;
		(end= line.find(separator, end)) != std::string::npos;
		++end)
	{
		parts.push_back(line.substr(start, end - start));
		start= end + 1;
	}
	parts.push_back(line.substr(start));
}

bool MllpV2Connection::parse(char const *message)
{
	bool accept= false;
	std::string buffer= message;

	if (buffer.length() < 9) {
		Log::log(LOG_WARNING, "Message is too short");
	} else if (buffer.substr(0, 4) != "MSH|") {
		Log::log(LOG_WARNING, "Message does not start with MSH header");
	} else {
		size_t lineOffset= buffer.find('\r', 0);
		if (lineOffset <= 0) {
			Log::log(LOG_WARNING, "Unable to find end of message line");
		} else {
			std::string line= buffer.substr(0, lineOffset);
			char fieldDelim= line.at(3);
			char componentDelim= line.at(4);

			std::vector<std::string> fields;
			split(line, fieldDelim, fields);

			if (fields.size() < 12) {
				Log::log(LOG_WARNING, "MSH line contains %d fields we need 12",
					fields.size());

				for (size_t i= 0; i < fields.size(); i++) {
					Log::log(LOG_DEBUG,
						"Field %d: %s",
						i, fields.at(i).c_str());
				}
			} else {
				fromApp= fields.at(2);
				fromFacility= fields.at(3);
				toApp= fields.at(4);
				toFacility= fields.at(5);
				messageId= fields.at(9);

				std::vector<std::string> components;
				split(fields.at(8), componentDelim, components);
				if (components.size() >= 2) {
					eventType= components.at(1);
				} else {
					eventType= "R01";
				}

				accept= true;
			}
		}
	}
	return accept;
}

void MllpV2Connection::acknowledge(AckType type)
{
	time_t now;
	time(&now);

	struct tm nowParts;
	gmtime_r(&now, &nowParts);

	char nowString[16];
	sprintf(nowString, "%04d%02d%02d%02d%02d%02d",
		nowParts.tm_year + 1900,
		nowParts.tm_mon + 1,
		nowParts.tm_mday,
		nowParts.tm_hour,
		nowParts.tm_min,
		nowParts.tm_sec);
	
	std::string response;
	response.append(1, 0x0B); // frame start

	response.append("MSH|^~\\&|");
	response.append(toApp);
	response.append("|");
	response.append(toFacility);
	response.append("|");
	response.append(fromApp);
	response.append("|");
	response.append(fromFacility);
	response.append("|");
	response.append(nowString);
	response.append("||ACK^");
	response.append(eventType);
	response.append("|");
	response.append(messageId);
	response.append("|P|2.4\r");

	response.append("MSA|");
	switch (type) {
	case AckType::ACCEPT:
		response.append("AA");
		break;
	case AckType::ERROR:
		response.append("AE");
		break;
	case AckType::REJECT:
		response.append("AR");
		break;
	}
	response.append("|");
	response.append(messageId);
	response.append("\r");
	response.append(1, 0x1c);
	response.append("\r");

	write(response.c_str(), response.length());
}


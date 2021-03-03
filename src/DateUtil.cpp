#include "system.h"
#include "DateUtil.h"

std::string DateUtil::TimeToISO8601(time_t ts)
{
	struct tm parts;
	gmtime_r(&ts, &parts);

	char buffer[32];
	sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02dZ",
		parts.tm_year + 1900,
		parts.tm_mon + 1,
		parts.tm_mday,
		parts.tm_hour,
		parts.tm_min,
		parts.tm_sec);

	return std::string(buffer);
}

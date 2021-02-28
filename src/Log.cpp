#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Log.h"

#define FORMAT_BUFFER_LEN 1023


pthread_mutex_t Log::lock= PTHREAD_MUTEX_INITIALIZER;
int Log::fd= 2;
int Log::logLevel= LOG_DEBUG;

void Log::open(char const *file)
{
	pthread_mutex_lock(&lock);

	if (fd != 2) {
		close(fd);
	}

	if (strcmp(file, "stderr") == 0) {
		fd= 2;
	} else {
		fd= ::open(file,
			O_CREAT | O_APPEND | O_CLOEXEC | O_WRONLY | O_CLOEXEC,
			S_IRUSR | S_IWUSR | S_IRGRP);
		if (fd == -1) {
			fprintf(stderr, "Error opening log file %s: %d: %s\n",
				file, errno, strerror(errno));
			fd= 2;
		}
	}

	pthread_mutex_unlock(&lock);
}

void Log::setLogLevel(int l)
{
	pthread_mutex_lock(&lock);
	logLevel= l;
	pthread_mutex_unlock(&lock);
}

void Log::log(int level, char const *format, ...)
{
	if (level >= logLevel) {
		char buffer[FORMAT_BUFFER_LEN + 1];
		int buffer_len;

		struct timeval now;
		gettimeofday(&now, NULL);

		struct tm *bt= localtime(&now.tv_sec);

		sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d ",
			bt->tm_year + 1900, bt->tm_mon + 1, bt->tm_mday,
			bt->tm_hour, bt->tm_min, bt->tm_sec);

		char *end= &buffer[strlen(buffer)];

		switch (level) {
		case LOG_DEBUG:
			strcpy(end, "[DEBUG]");
			break;
		case LOG_INFO:
			strcpy(end, "[INFO]");
			break;
		case LOG_WARNING:
			strcpy(end, "[WARNING]");
			break;
		case LOG_ERROR:
			strcpy(end, "[ERROR]");
			break;
		case LOG_CRITICAL:
			strcpy(end, "[ASSERT]");
			break;

		default:
			strcpy(end, "[WTF]");
			break;
		}

		va_list args;
		va_start(args, format);

		end= &buffer[strlen(buffer)];
		*(end++)= ' ';

		buffer_len= FORMAT_BUFFER_LEN - (end - buffer) - (1 /* for NL */);
		vsnprintf(end, buffer_len, format, args);
		buffer_len= strlen(buffer);
		buffer[buffer_len++]= '\n';
		buffer[buffer_len]= '\0';
		va_end(args);

		pthread_mutex_lock(&lock);
		int nwrote= write(fd, buffer, buffer_len);
		pthread_mutex_unlock(&lock);

		if (nwrote == -1) {
			fprintf(stderr,
				"LOG FAILURE: %d: %s\n", errno, strerror(errno));
		} else if (nwrote != buffer_len) {
			fprintf(stderr,
				"LOG INCOMPLETE: %d/%d written\n", nwrote, buffer_len);
		}
	}
}


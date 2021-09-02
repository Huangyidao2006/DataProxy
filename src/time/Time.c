/**
 * Created by huangjian on 2021/9/2.
 */

#include "time/Time.h"

#include <sys/time.h>
#include <time.h>
#include <strings.h>

long long currentTimeMillis() {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int getCurrentTime(const char* fmt, char* buffer, int len) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);

	bzero(buffer, len);
	strftime(buffer, len, fmt, t);

	return 0;
}

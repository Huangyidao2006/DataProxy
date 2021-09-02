/**
 * Created by huangjian on 2021/9/2.
 */

#include "log/Log.h"
#include "time/Time.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <syscall.h>
#include <unistd.h>

#define MAX_LOG_SIZE 10240

static LogLevel g_logLevel = SDK_INFO;

void setLogLevel(LogLevel level) {
	g_logLevel = level;
}

void printLog(LogLevel level, const char* filename, int line, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char* buffer = (char*) malloc (MAX_LOG_SIZE);
	vsnprintf(buffer, MAX_LOG_SIZE, fmt, args);

	va_end(args);

	char curTime[100] = {0};
	getCurrentTime("%Y-%m-%d %H:%M:%S", curTime, sizeof(curTime));
	long curTimeMs = currentTimeMillis() % 1000;

	printf("[%s %d T0x%08x] %s(line %d): %s\n", curTime, curTimeMs, syscall(__NR_gettid),
		   filename, line, buffer);

	free(buffer);
}

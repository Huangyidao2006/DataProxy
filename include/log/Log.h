/**
 * Created by huangjian on 2021/9/2.
 */

#ifndef DATAPROXY_LOG_H
#define DATAPROXY_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SDK_DEBUG,
	SDK_INFO,
	SDK_WARN,
	SDK_ERROR
} LogLevel;

#define LOG_D(...) \
	printLog(SDK_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_I(...) \
	printLog(SDK_INFO, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_W(...) \
	printLog(SDK_WARN, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_E(...) \
	printLog(SDK_ERROR, __FILE__, __LINE__, __VA_ARGS__)

void setLogLevel(LogLevel level);

void printLog(LogLevel level, const char* filename, int line, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif//DATAPROXY_LOG_H

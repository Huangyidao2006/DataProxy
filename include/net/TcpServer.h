/**
 * Created by huangjian on 2021/9/3.
 */

#ifndef DATAPROXY_TCPSERVER_H
#define DATAPROXY_TCPSERVER_H

#include "common/Common.h"

#include <stdbool.h>
#include <pthread.h>

#define MAX_CLIENT_COUNT 100

typedef struct {
	int fd;
	char ip[IP_CHARS_LEN];
	int port;
} ClientInfo;

typedef void (*TcpServerAcceptCbFunc)(int fd, const char* ip, unsigned short port, void* pCtx);
typedef void (*TcpServerRecvCbFunc)(const ClientInfo* info, const char* data, int len, void* pCtx);
typedef void (*TcpServerErrorCbFunc)(const ClientInfo* info, int error, const char* des, void* pCtx);

typedef struct {
	unsigned short local_port;
	int socket_fd;
	bool is_start;
	bool is_stop;
	ClientInfo clients[MAX_CLIENT_COUNT];
	int client_count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	TcpServerAcceptCbFunc accept_cb;
	TcpServerRecvCbFunc recv_cb;
	TcpServerErrorCbFunc error_cb;
} TcpServerCtx;

int TcpServerInit(TcpServerCtx* pCtx);

int TcpServerStart(TcpServerCtx* pCtx);

int TcpServerSend(TcpServerCtx* pCtx, int remoteFd, const char* data, int len);

int TcpServerStop(TcpServerCtx* pCtx);

int TcpServerRelease(TcpServerCtx* pCtx);

#endif//DATAPROXY_TCPSERVER_H

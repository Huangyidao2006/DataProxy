/**
 * Created by huangjian on 2021/9/2.
 */

#ifndef DATAPROXY_TCPCLIENT_H
#define DATAPROXY_TCPCLIENT_H

#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>

typedef void (*TcpClientRecvCbFunc)(const char* data, int len, void* pCtx);
typedef void (*TcpClientErrorCbFunc)(int error, const char* des, void* pCtx);

typedef struct {
	char server_ip[16];
	unsigned short server_port;
	unsigned short local_port;
	int socket_fd;
	bool is_noblock;
	bool is_stop_recv;
	char* recv_buffer;
	int recv_buffer_len;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
    TcpClientRecvCbFunc recv_cb;
    TcpClientErrorCbFunc error_cb;
} TcpClientCtx;

int TcpClientInit(TcpClientCtx* pCtx);

int TcpClientConnect(TcpClientCtx* pCtx);

int TcpClientSend(TcpClientCtx* pCtx, const char* data, int len);

int TcpClientClose(TcpClientCtx* pCtx);

int TcpClientRelease(TcpClientCtx* pCtx);

#endif//DATAPROXY_TCPCLIENT_H

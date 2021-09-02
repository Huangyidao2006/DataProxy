/**
 * Created by huangjian on 2021/9/2.
 */

#ifndef DATAPROXY_TCPCLIENT_H
#define DATAPROXY_TCPCLIENT_H

#include <sys/socket.h>

typedef struct _TcpClientCtx {
	char server_ip[16];
	unsigned short server_port;
	unsigned short local_port;
	int socket_fd;
} TcpClientCtx;

int TcpClientCreateSocket(TcpClientCtx* pCtx);

int TcpClientConnect(TcpClientCtx* pCtx);

int TcpClientSend(TcpClientCtx* pCtx, const char* data, int len);

int TcpClientClose(TcpClientCtx* pCtx);

#endif//DATAPROXY_TCPCLIENT_H

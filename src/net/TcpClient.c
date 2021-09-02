/**
 * Created by huangjian on 2021/9/2.
 */

#include "net/TcpClient.h"
#include "Error.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <strings.h>

int TcpClientCreateSocket(TcpClientCtx* pCtx) {
	if (NULL == pCtx) {
		return ERROR_NULL_PTR;
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return ERROR_FAILED;
	}

	pCtx->socket_fd = fd;

	struct sockaddr_in localAddr;
	bzero(&localAddr, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddr.sin_port = htons(pCtx->local_port);

	int ret = bind(fd, (struct sockaddr*) &localAddr, sizeof(localAddr));

	return ret;
}

int TcpClientConnect(TcpClientCtx* pCtx) {
	if (NULL == pCtx) {
		return ERROR_NULL_PTR;
	}

	struct sockaddr_in serverAddr;
	bzero(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(pCtx->server_ip);
	serverAddr.sin_port = pCtx->server_port;

	int ret = connect(pCtx->socket_fd, (struct sockaddr*) &serverAddr, sizeof(serverAddr));

	return ret;
}

int TcpClientSend(TcpClientCtx* pCtx, const char* data, int len);

int TcpClientClose(TcpClientCtx* pCtx);

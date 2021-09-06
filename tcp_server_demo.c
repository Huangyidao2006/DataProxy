/**
 * Created by huangjian on 2021/9/3.
 */

#include "log/Log.h"
#include "net/TcpServer.h"
#include "common/Error.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void TcpServerAcceptCb(int fd, const char* ip, unsigned short port, void* pCtx) {
	LOG_D("client connected, fd=%d, ip=%s, port=%d", fd, ip, port);
}

void TcpServerRecvCb(const ClientInfo* info, const char* data, int len, void* pCtx) {
	char* buffer = (char*) malloc(len + 1);
	memcpy(buffer, data, len);
	buffer[len] = '\0';

	LOG_D("recv from fd=%d, ip=%s, port=%d, content=%s, len=%d",
		  info->fd, info->ip, info->port, buffer, len);

	free(buffer);
}

void TcpServerErrorCb(const ClientInfo* info, int error, const char* des, void* pCtx) {
	LOG_D("error=%d, %s", error, des);
}

int main(int argc, char* argv[]) {
	TcpServerCtx tcpServerCtx;
	bzero(&tcpServerCtx, sizeof(tcpServerCtx));
	tcpServerCtx.local_port = 0x1234;
	tcpServerCtx.accept_cb = TcpServerAcceptCb;
	tcpServerCtx.recv_cb = TcpServerRecvCb;
	tcpServerCtx.error_cb = TcpServerErrorCb;

	int ret = TcpServerInit(&tcpServerCtx);
	LOG_D("init, ret=%d, fd=%d, port=%d", ret, tcpServerCtx.socket_fd, tcpServerCtx.local_port);

	if (SUCCESS != ret) {
		return 1;
	}

	ret = TcpServerStart(&tcpServerCtx);
	LOG_D("start, ret=%d", ret);

	if (SUCCESS != ret) {
		return 2;
	}

	char buffer[1024];
	while (true) {
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, sizeof(buffer), stdin);
		int len = strlen(buffer);
		buffer[len - 1] = '\0';

		if (strcmp(buffer, "exit") == 0) {
			break;
		}
	}

	TcpServerStop(&tcpServerCtx);
	TcpServerRelease(&tcpServerCtx);

	return 0;
}

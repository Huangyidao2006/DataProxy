/**
 * Created by huangjian on 2021/9/3.
 */

#include "log/Log.h"
#include "net/TcpClient.h"
#include "common/Error.h"
#include "common/Common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void TcpClientRecvCb(const char* data, int len, void* pCtx) {
	char* buffer = (char*) malloc(len + 1);
	memcpy(buffer, data, len);
	buffer[len] = '\0';

	LOG_D("recv, content=%s, len=%d", buffer, len);

	free(buffer);
}

void TcpClientErrorCb(int error, const char* des, void* pCtx) {
	LOG_D("error=%d, %s", error, des);
}

int main(int argc, char* argv[]) {
	TcpClientCtx tcpClientCtx;
	bzero(&tcpClientCtx, sizeof(tcpClientCtx));
	tcpClientCtx.local_port = 0x2345;
	tcpClientCtx.recv_buffer = (char*) malloc(64 * 1024);
	tcpClientCtx.recv_buffer_len = 64 * 1024;
	tcpClientCtx.recv_cb = TcpClientRecvCb;
	tcpClientCtx.error_cb = TcpClientErrorCb;

	int ret = TcpClientInit(&tcpClientCtx);
	LOG_D("init, ret=%d, fd=%d", ret, tcpClientCtx.socket_fd);

	if (SUCCESS != ret) {
		return 1;
	}

	ret = TcpClientConnect(&tcpClientCtx, "127.0.0.1", 0x1234);
	LOG_D("connect, ret=%d", ret);

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

		LOG_D("input: %s, len=%d", buffer, len);

		ret = TcpClientSend(&tcpClientCtx,  buffer, len);
		LOG_D("send, ret=%d", ret);
	}

	TcpClientClose(&tcpClientCtx);
	TcpClientRelease(&tcpClientCtx);

	return 0;
}

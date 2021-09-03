/**
 * Created by huangjian on 2021/9/2.
 */

#include "log/Log.h"
#include "net/UdpHelper.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void UdpRecvCb(const char* data, int len, void* pCtx) {
	char* s = (char*) malloc(len + 1);
	memcpy(s, data, len);
	s[len] = '\0';

    LOG_D("recv: %s, len=%d", s, len);

	free(s);
}

void UdpErrorCb(int error, const char* des, void* pCtx) {
    LOG_D("error=%d, %s", error, des);
}

int main(int argc, char* argv[]) {
	UdpHelperCtx udpCtx;
	udpCtx.local_port = 5678;
	udpCtx.recv_buffer = (char*) malloc (64 * 1024);
	udpCtx.recv_buffer_len = 64 * 1024;
	udpCtx.is_stop_recv = false;
	udpCtx.recv_cb = UdpRecvCb;
	udpCtx.error_cb = UdpErrorCb;

	int ret = UdpHelperInit(&udpCtx);
	LOG_D("init, ret=%d", ret);

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

		ret = UdpHelperSend(&udpCtx, "127.0.0.1", 1234, buffer, len);
		LOG_D("send, ret=%d", ret);
	}

	UdpHelperClose(&udpCtx);
	UdpHelperRelease(&udpCtx);

	return 0;
}

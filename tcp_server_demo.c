/**
 * Created by huangjian on 2021/9/3.
 */

#include "log/Log.h"
#include "net/TcpServer.h"
#include "common/Error.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SEND_FRAME_LEN (1024 * 100)

bool g_isSend = false;

typedef struct {
	ClientInfo* pClientInfo;
	TcpServerCtx* pServerCtx;
} SendCtx;

void* send_thread_func(void* params) {
	SendCtx* pSendCtx = (SendCtx*) params;

	FILE* fp = fopen("../assets/yueyawan.mp3", "rb");
	if (fp != NULL) {
		char buffer[SEND_FRAME_LEN];
		while (true) {
			size_t len = fread(buffer, 1, sizeof(buffer), fp);
			if (len > 0) {
				int ret = TcpServerSend(pSendCtx->pServerCtx, pSendCtx->pClientInfo->fd, buffer, len);
				if (ret != len) {
					LOG_E("TcpServerSend, ret=%d", ret);
					break;
				}
			}

			if (len != sizeof(buffer)) {
				LOG_D("endofmusic");

				sleepMs(100);

				char* endFlag = "endofmusic";
				TcpServerSend(pSendCtx->pServerCtx, pSendCtx->pClientInfo->fd, endFlag, strlen(endFlag));
				break;
			}

			sleepMs(10);
		}

		fclose(fp);
	} else {
		LOG_E("open %s failed", "yueyawan.mp3");
	}

	free(pSendCtx);

	g_isSend = false;
}

void TcpServerAcceptCb(int fd, const char* ip, unsigned short port, void* pCtx) {
	LOG_D("client connected, fd=%d, ip=%s, port=%d", fd, ip, port);
}

void TcpServerRecvCb(const ClientInfo* info, const char* data, int len, void* pCtx) {
	char* buffer = (char*) malloc(len + 1);
	memcpy(buffer, data, len);
	buffer[len] = '\0';

	LOG_D("recv from fd=%d, ip=%s, port=%d, content=%s, len=%d",
		  info->fd, info->ip, info->port, buffer, len);

	if (strcmp(buffer, "music") == 0) {
		if (!g_isSend) {
			g_isSend = true;

			LOG_D("try to send music");
			char* startFlag = "startofmusic";
			TcpServerSend(pCtx, info->fd, startFlag, strlen(startFlag));

			SendCtx* pSendCtx = (SendCtx*) malloc(sizeof(SendCtx));
			pSendCtx->pClientInfo = info;
			pSendCtx->pServerCtx = pCtx;

			pthread_t tid;
			pthread_create(&tid, NULL, send_thread_func, pSendCtx);
		}
	}

	free(buffer);
}

void TcpServerErrorCb(const ClientInfo* info, int error, const char* des, void* pCtx) {
	LOG_D("error=%d, %s", error, des);
}

int main(int argc, char* argv[]) {
	TcpServerCtx tcpServerCtx;
	bzero(&tcpServerCtx, sizeof(tcpServerCtx));
	tcpServerCtx.local_port = 5678;
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

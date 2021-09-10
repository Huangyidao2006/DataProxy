/**
 * Created by huangjian on 2021/9/2.
 */

#include "log/Log.h"
#include "net/UdpHelper.h"
#include "common/Common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SEND_FRAME_LEN (1024 * 50)

bool g_isSend = false;

typedef struct {
    char ip[IP_CHARS_LEN];
	unsigned short port;
    UdpHelperCtx * pUdpHelperCtx;
} SendCtx;

void* send_thread_func(void* params) {
    SendCtx* pSendCtx = (SendCtx*) params;

    FILE* fp = fopen("../assets/yueyawan.mp3", "rb");
    if (fp != NULL) {
        char buffer[SEND_FRAME_LEN];
        while (true) {
            size_t len = fread(buffer, 1, sizeof(buffer), fp);
            if (len > 0) {
                int ret = UdpHelperSend(pSendCtx->pUdpHelperCtx, pSendCtx->ip, pSendCtx->port, buffer, len);
                if (ret != len) {
                    LOG_E("UdpHelperSend, ret=%d", ret);
                    break;
                }
            }

            if (len != sizeof(buffer)) {
                LOG_D("endofmusic");

                sleepMs(100);

                char* endFlag = "endofmusic";
                UdpHelperSend(pSendCtx->pUdpHelperCtx, pSendCtx->ip, pSendCtx->port, endFlag, strlen(endFlag));
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

void UdpRecvCb(const char* ip, unsigned short port, const char* data, int len, void* pCtx) {
	char* buffer = (char*) malloc(len + 1);
	memcpy(buffer, data, len);
    buffer[len] = '\0';

    LOG_D("recvfrom ip=%s, port=%d, len=%d", ip, port, len);

    if (strcmp(buffer, "music") == 0) {
        if (!g_isSend) {
            g_isSend = true;

            LOG_D("try to send music");
            char* startFlag = "startofmusic";
            UdpHelperSend(pCtx, ip, port, startFlag, strlen(startFlag));

            SendCtx* pSendCtx = (SendCtx*) malloc(sizeof(SendCtx));
            memcpy(pSendCtx->ip, ip, IP_CHARS_LEN);
            pSendCtx->port = port;
			pSendCtx->pUdpHelperCtx = pCtx;

            pthread_t tid;
            pthread_create(&tid, NULL, send_thread_func, pSendCtx);
        }
    }

	free(buffer);
}

void UdpErrorCb(int error, const char* des, void* pCtx) {
    LOG_D("error=%d, %s", error, des);
}

int main(int argc, char* argv[]) {
	UdpHelperCtx udpCtx;
	udpCtx.local_port = 2345;
	udpCtx.recv_cb = UdpRecvCb;
	udpCtx.error_cb = UdpErrorCb;

	int ret = UdpHelperInit(&udpCtx);
	LOG_D("init, ret=%d, fd=%d, port=%d", ret, udpCtx.socket_fd, udpCtx.local_port);

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

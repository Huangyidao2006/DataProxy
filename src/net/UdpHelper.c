//
// Created by hj on 2021/9/2.
//

#include "net/UdpHelper.h"
#include "common/Common.h"
#include "common/Error.h"
#include "log/Log.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>

#define MAX_TRY_COUNT 20
#define DEF_RECV_BUFFER_LEN (1024 * 64)

static void* recv_thread_func(void* params) {
	UdpHelperCtx* pCtx = (UdpHelperCtx*) params;

	int count = 0;
	int fd = pCtx->socket_fd;

	while (!pCtx->is_stop_recv) {
		fd_set readFds;
		FD_ZERO(&readFds);
		FD_SET(fd, &readFds);
		struct timeval tv = {0, 500 * 1000};

		int ret = select(fd + 1, &readFds, NULL, NULL, &tv);
		if (ret > 0) {
			struct sockaddr_in oppoAddr;
			int addrLen = sizeof(oppoAddr);
			bzero(&oppoAddr, addrLen);

			int len = recvfrom(fd, pCtx->recv_buffer, pCtx->recv_buffer_len, 0,
							   (struct sockaddr*) &oppoAddr, &addrLen);
			if (len > 0) {
				count = 0;

				char remoteIP[IP_CHARS_LEN] = {0};
				inet_ntop(AF_INET, &oppoAddr.sin_addr.s_addr, remoteIP, sizeof(remoteIP));
				unsigned short remotePort = ntohs(oppoAddr.sin_port);

				pCtx->recv_cb(remoteIP, remotePort, pCtx->recv_buffer, len, pCtx);
			} else if (0 == len) {
				pCtx->error_cb(ERROR_SOCK_REMOTE_CLOSE, "remote socket closed", pCtx);

				pthread_mutex_lock(&(pCtx->mutex));

				pCtx->is_stop_recv = true;

				pthread_mutex_unlock(&(pCtx->mutex));
			} else {
				if ((EAGAIN == errno || EINTR == errno) && count < MAX_TRY_COUNT) {
					count++;
					sleepMs(10);
				} else {
					goto error;
				}
			}
		} else if (ret < 0) {
			goto error;
		}
	}

	goto exit;

error:
	LOG_E("errno=%d, %s", errno, strerror(errno));

	pCtx->error_cb(ERROR_SOCK_RECV, "recv error", pCtx);

exit:

	pthread_mutex_lock(&(pCtx->mutex));

	pCtx->is_stop_recv = true;

	pthread_cond_signal(&(pCtx->cond));

	pthread_mutex_unlock(&(pCtx->mutex));

	return NULL;
}

int UdpHelperInit(UdpHelperCtx* pCtx) {
	if (NULL == pCtx) {
		return ERROR_NULL_PTR;
	}

	pCtx->recv_buffer = NULL;

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		LOG_E("errno=%d, %s", errno, strerror(errno));
		return ERROR_FAILED;
	}

    int flag = 1, len = sizeof(int);
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, len)) {
        LOG_E("errno=%d, %s", errno, strerror(errno));
        return ERROR_FAILED;
    }

    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flag, len)) {
        LOG_E("errno=%d, %s", errno, strerror(errno));
        return ERROR_FAILED;
    }

	pCtx->socket_fd = fd;

	struct sockaddr_in localAddr;
	bzero(&localAddr, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddr.sin_port = htons(pCtx->local_port);

	int ret = bind(fd, (struct sockaddr*) &localAddr, sizeof(localAddr));
	if (ret < 0) {
		LOG_E("errno=%d, %s", errno, strerror(errno));
	}

	int flags = fcntl(pCtx->socket_fd, F_GETFL, 0);
	ret = fcntl(pCtx->socket_fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0) {
		LOG_E("errno=%d, %s", errno, strerror(errno));
	}

	if (NULL == pCtx->recv_buffer) {
		pCtx->recv_buffer = (char*) malloc(DEF_RECV_BUFFER_LEN);
		pCtx->recv_buffer_len = DEF_RECV_BUFFER_LEN;
	}

	pCtx->is_stop_recv = false;

	pthread_mutex_init(&(pCtx->mutex), NULL);
	pthread_cond_init(&(pCtx->cond), NULL);

	pthread_t tid;
	pthread_create(&tid, NULL, recv_thread_func, pCtx);

	return SUCCESS;
}

int UdpHelperSend(UdpHelperCtx* pCtx, const char* toIP, unsigned short toPort,
                  const char* data, int len) {
    if (NULL == pCtx) {
        return ERROR_NULL_PTR;
    }

    if (pCtx->socket_fd <= 0) {
        LOG_E("socket_fd is invalid");
        return ERROR_INVALID_FD;
    }

	struct sockaddr_in remoteAddr;
	bzero(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_addr.s_addr = inet_addr(toIP);
	remoteAddr.sin_port = htons(toPort);

	int sent = sendto(pCtx->socket_fd, data, len, 0,
					  (struct sockaddr*) &remoteAddr, sizeof(remoteAddr));
	if (sent < 0) {
        LOG_E("errno=%d, %s", errno, strerror(errno));
	}

	return sent;
}

int UdpHelperClose(UdpHelperCtx* pCtx) {
    if (NULL == pCtx) {
        LOG_E("pCtx is NULL");
        return ERROR_NULL_PTR;
    }

    if (pCtx->socket_fd <= 0) {
        LOG_E("socket_fd is invalid");
        return ERROR_INVALID_FD;
    }

    pthread_mutex_lock(&(pCtx->mutex));

    if (!pCtx->is_stop_recv) {
        pCtx->is_stop_recv = true;

        pthread_cond_wait(&(pCtx->cond), &(pCtx->mutex));
    }

    pthread_mutex_unlock(&(pCtx->mutex));

    pCtx->socket_fd = -1;
    close(pCtx->socket_fd);

    return SUCCESS;
}

int UdpHelperRelease(UdpHelperCtx* pCtx) {
    if (NULL == pCtx) {
        LOG_E("pCtx is NULL");
        return ERROR_NULL_PTR;
    }

    pthread_cond_destroy(&(pCtx->cond));
    pthread_mutex_destroy(&(pCtx->mutex));

    free(pCtx->recv_buffer);

    return SUCCESS;
}

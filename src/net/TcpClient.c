/**
 * Created by huangjian on 2021/9/2.
 */

#include "net/TcpClient.h"
#include "common/Common.h"
#include "common/Error.h"
#include "log/Log.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_TRY_COUNT 20
#define DEF_RECV_BUFFER_LEN (1024 * 1024)

int TcpClientInit(TcpClientCtx* pCtx) {
	if (NULL == pCtx) {
		return ERROR_NULL_PTR;
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0);
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

	pCtx->is_stop_recv = false;

	if (NULL == pCtx->recv_buffer) {
		pCtx->recv_buffer = (char*) malloc(DEF_RECV_BUFFER_LEN);
		pCtx->recv_buffer_len = DEF_RECV_BUFFER_LEN;
	}

	pthread_mutex_init(&(pCtx->mutex), NULL);
	pthread_cond_init(&(pCtx->cond), NULL);

	return ret;
}

static void* recv_thread_func(void* params) {
	LOG_I("receive thread started");

    TcpClientCtx* pCtx = (TcpClientCtx*) params;

	int count = 0;
	int fd = pCtx->socket_fd;

	while (!pCtx->is_stop_recv) {
		fd_set readFds;
		FD_ZERO(&readFds);
		FD_SET(fd, &readFds);
        struct timeval tv = {1, 0};

		int ret = select(fd + 1, &readFds, NULL, NULL, &tv);
		if (ret > 0) {
            int len = recv(fd, pCtx->recv_buffer, pCtx->recv_buffer_len, 0);
            if (len > 0) {
                count = 0;

                pCtx->recv_cb(pCtx->recv_buffer, len, pCtx);
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

	LOG_I("receive thread stopped");

	return NULL;
}

int TcpClientConnect(TcpClientCtx* pCtx, const char* ip, unsigned short port) {
	if (NULL == pCtx) {
		LOG_E("pCtx is NULL");
		return ERROR_NULL_PTR;
	}

	snprintf(pCtx->server_ip, IP_CHARS_LEN, "%s", ip);
	pCtx->server_port = port;

	struct sockaddr_in serverAddr;
	bzero(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(pCtx->server_ip);
	serverAddr.sin_port = htons(pCtx->server_port);

	LOG_D("connecting %s:%d", ip, port);

	int ret = connect(pCtx->socket_fd, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        LOG_E("errno=%d, %s", errno, strerror(errno));
    }

	int flags = fcntl(pCtx->socket_fd, F_GETFL, 0);
	ret = fcntl(pCtx->socket_fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0) {
		LOG_E("errno=%d, %s", errno, strerror(errno));
	}

	pthread_t tid;
	pthread_create(&tid, NULL, recv_thread_func, pCtx);

	return ret;
}

int TcpClientSend(TcpClientCtx* pCtx, const char* data, int len) {
    if (NULL == pCtx) {
        LOG_E("pCtx is NULL");
        return ERROR_NULL_PTR;
    }

	if (pCtx->socket_fd <= 0) {
        LOG_E("socket_fd is invalid");
		return ERROR_INVALID_FD;
	}

	int count = 0;
    int offset = 0;
	while (offset != len) {
		int sent = send(pCtx->socket_fd, &data[offset], len - offset, 0);
        if (sent > 0) {
            offset += sent;
			count = 0;
		} else {
			if ((EAGAIN == errno || EINTR == errno) && count < MAX_TRY_COUNT) {
				count++;
                sleepMs(10);
			} else {
                LOG_E("errno=%d, %s", errno, strerror(errno));

                pCtx->error_cb(ERROR_SOCK_SEND, "send error", pCtx);
				break;
			}
		}
	}

	return offset;
}

int TcpClientClose(TcpClientCtx* pCtx) {
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

	close(pCtx->socket_fd);
	pCtx->socket_fd = -1;

	return SUCCESS;
}

int TcpClientRelease(TcpClientCtx* pCtx) {
    if (NULL == pCtx) {
        LOG_E("pCtx is NULL");
        return ERROR_NULL_PTR;
    }

	pthread_cond_destroy(&(pCtx->cond));
	pthread_mutex_destroy(&(pCtx->mutex));

	free(pCtx->recv_buffer);

	return SUCCESS;
}

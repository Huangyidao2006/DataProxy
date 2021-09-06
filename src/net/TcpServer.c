/**
 * Created by huangjian on 2021/9/3.
 */

#include "net/TcpServer.h"
#include "common/Error.h"
#include "log/Log.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>

#define RECV_BUFFER_LEN (1024 * 64)
#define MAX_TRY_COUNT 20

static ClientInfo* getClientInfoByFd(ClientInfo* pClientInfos, int fd) {
	for (int i = 0; i < MAX_CLIENT_COUNT; i++) {
		if (pClientInfos[i].fd == fd) {
			return &pClientInfos[i];
		}
	}

	return NULL;
}

static ClientInfo* findEmptyClientInfoUnit(ClientInfo* pClientInfos) {
	return getClientInfoByFd(pClientInfos, -1);
}

static bool addClientInfo(ClientInfo* pClientInfos, int fd, const char* ip, unsigned short port) {
	ClientInfo* pInfo = findEmptyClientInfoUnit(pClientInfos);
	if (NULL == pInfo) {
		return false;
	}

	pInfo->fd = fd;
	pInfo->port = port;
	memcpy(pInfo->ip, ip, IP_CHARS_LEN);

	return true;
}

static void deleteClientInfoByFd(ClientInfo* pClientInfos, int fd) {
	ClientInfo* pInfo = getClientInfoByFd(pClientInfos, fd);
	if (NULL != pInfo) {
		pInfo->fd = -1;
		pInfo->port = 0;
		bzero(pInfo->ip, IP_CHARS_LEN);
	}
}

int TcpServerInit(TcpServerCtx* pCtx) {
	if (NULL == pCtx) {
		return ERROR_NULL_PTR;
	}

	for (int i = 0; i < MAX_CLIENT_COUNT; i++) {
		pCtx->clients[i].fd = -1;
		bzero(pCtx->clients[i].ip, IP_CHARS_LEN);
		pCtx->clients[i].port = 0;
	}

	pCtx->client_count = 0;
	pCtx->is_start = false;
	pCtx->is_stop = false;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
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

	pthread_mutex_init(&(pCtx->mutex), NULL);
	pthread_cond_init(&(pCtx->cond), NULL);

	return ret;
}

static void* server_thread_func(void* params) {
	TcpServerCtx* pCtx = (TcpServerCtx*) params;
	char* recvBuffer = (char*) malloc(RECV_BUFFER_LEN);

	while (!pCtx->is_stop) {
		fd_set readFds;
		FD_ZERO(&readFds);

		const int clientCount = pCtx->client_count + 1;
		int clientFds[clientCount];
		int j = 0;
		clientFds[j++] = pCtx->socket_fd;
		int maxFd = clientFds[0];
		FD_SET(clientFds[0], &readFds);

		for (int i = 0; i < MAX_CLIENT_COUNT; i++) {
			int fd = pCtx->clients[i].fd;
			if (-1 != fd) {
				if (fd > maxFd) {
					maxFd = fd;
				}

				clientFds[j++] = fd;
				FD_SET(fd, &readFds);
			}
		}

		struct timeval tv = {1, 0};

		int ret = select(maxFd + 1, &readFds, NULL, NULL, &tv);
		if (ret > 0) {
			for (int i = 1; i < clientCount; i++) {
				int curFd = clientFds[i];
				if (FD_ISSET(curFd, &readFds)) {
					ClientInfo* curInfo = getClientInfoByFd(pCtx->clients, curFd);
					ret = recv(curFd, recvBuffer, RECV_BUFFER_LEN, 0);

					if (ret > 0) {
						pCtx->recv_cb(curInfo, recvBuffer, ret);
					} else if (0 == ret) {
						pCtx->error_cb(curInfo, ERROR_SOCK_REMOTE_CLOSE, "remote socket closed", pCtx);

						close(curFd);
						deleteClientInfoByFd(pCtx->clients, curFd);
						pCtx->client_count--;
					} else {
						if (EAGAIN == errno || EINTR == errno) {

						} else {
							pCtx->error_cb(curInfo, ERROR_SOCK_RECV, "receive error", pCtx);

							close(curFd);
							deleteClientInfoByFd(pCtx->clients, curFd);
							pCtx->client_count--;
						}
					}
				}
			}

			if (FD_ISSET(clientFds[0], &readFds)) {
				// accept a client
				struct sockaddr_in clientAddr;
				socklen_t addrLen = sizeof(clientAddr);
				bzero(&clientAddr, addrLen);

				int remoteFd = accept(clientFds[0], (struct sockaddr*) &clientAddr, &addrLen);
				LOG_D("accept, ret=%d", remoteFd);

				if (remoteFd > 0) {
					char ip[IP_CHARS_LEN] = {0};
					inet_ntop(AF_INET, &clientAddr, ip, sizeof(ip));
					unsigned short port = ntohs(clientAddr.sin_port);

					LOG_D("client connected");

					bool isAdded = addClientInfo(pCtx->clients, remoteFd, ip, port);
					if (isAdded) {
						pCtx->client_count++;

						int flags = fcntl(remoteFd, F_GETFL, 0);
						ret = fcntl(remoteFd, F_SETFL, flags | O_NONBLOCK);
						if (ret < 0) {
							LOG_E("errno=%d, %s", errno, strerror(errno));
						}

						pCtx->accept_cb(remoteFd, ip, port, pCtx);
					} else {
						close(remoteFd);

						pCtx->error_cb(NULL, ERROR_SOCK_TOO_MANY_CLIENTS, "too many clients");
					}
				} else {
					LOG_E("errno=%d, %s", errno, strerror(errno));

					pCtx->error_cb(NULL, ERROR_SOCK_SEND, "accept error");

					goto error;
				}
			}
		} else if (ret == 0) {
//			LOG_D("select, no readable fd");
		} else if (ret < 0) {
			LOG_E("errno=%d, %s", errno, strerror(errno));

			pCtx->error_cb(NULL, ERROR_SELECT, "select error");

			goto error;
		}
	} // while

error:
	free(recvBuffer);

	pthread_mutex_lock(&pCtx->mutex);

	pCtx->is_stop = true;

	pthread_cond_signal(&pCtx->cond);

	pthread_mutex_unlock(&pCtx->mutex);

	return NULL;
}

int TcpServerStart(TcpServerCtx* pCtx) {
	if (NULL == pCtx) {
		LOG_E("pCtx is NULL");
		return ERROR_NULL_PTR;
	}

	if (pCtx->socket_fd <= 0) {
		LOG_E("socket_fd is invalid");
		return ERROR_INVALID_FD;
	}

	if (pCtx->is_start) {
		LOG_E("already started, invalid operation");
		return ERROR_INVALID_OPERATION;
	}

	int ret = listen(pCtx->socket_fd, 20);
	if (ret < 0) {
		LOG_E("errno=%d, %s", errno, strerror(errno));

		return ERROR_SOCK_LISTEN;
	}

	pthread_t tid;
	pthread_create(&tid, NULL, server_thread_func, pCtx);

	pCtx->is_start = true;
	pCtx->is_stop = false;

	return SUCCESS;
}

int TcpServerSend(TcpServerCtx* pCtx, int remoteFd, const char* data, int len) {
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
		int sent = send(remoteFd, &data[offset], len - offset, 0);
		if (sent > 0) {
			offset += sent;
			count = 0;
		} else {
			if ((EAGAIN == errno || EINTR == errno) && count < MAX_TRY_COUNT) {
				count++;
				sleepMs(10);
			} else {
				LOG_E("errno=%d, %s", errno, strerror(errno));

				pCtx->error_cb(NULL, ERROR_SOCK_SEND, "send error");
				break;
			}
		}
	}

	return len;
}

int TcpServerStop(TcpServerCtx* pCtx) {
	if (NULL == pCtx) {
		LOG_E("pCtx is NULL");
		return ERROR_NULL_PTR;
	}

	if (pCtx->socket_fd <= 0) {
		LOG_E("socket_fd is invalid");
		return ERROR_INVALID_FD;
	}

	pthread_mutex_lock(&(pCtx->mutex));

	if (!pCtx->is_stop) {
		pCtx->is_stop = true;

		pthread_cond_wait(&(pCtx->cond), &(pCtx->mutex));
	}

	pthread_mutex_unlock(&(pCtx->mutex));

	close(pCtx->socket_fd);
	pCtx->socket_fd = -1;

	return SUCCESS;
}

int TcpServerRelease(TcpServerCtx* pCtx) {
	if (NULL == pCtx) {
		LOG_E("pCtx is NULL");
		return ERROR_NULL_PTR;
	}

	pthread_cond_destroy(&(pCtx->cond));
	pthread_mutex_destroy(&(pCtx->mutex));

	return SUCCESS;
}

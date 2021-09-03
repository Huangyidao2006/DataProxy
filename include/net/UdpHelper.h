//
// Created by hj on 2021/9/2.
//

#ifndef DATAPROXY_UDPHELPER_H
#define DATAPROXY_UDPHELPER_H

#include <stdbool.h>
#include <pthread.h>

typedef void (*UdpHelperRecvCbFunc)(const char* data, int len, void* pCtx);
typedef void (*UdpHelperErrorCbFunc)(int error, const char* des, void* pCtx);

typedef struct {
    int socket_fd;
    unsigned short local_port;
    bool is_stop_recv;
    char* recv_buffer;
    int recv_buffer_len;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
	UdpHelperRecvCbFunc recv_cb;
	UdpHelperErrorCbFunc error_cb;
} UdpHelperCtx;

int UdpHelperInit(UdpHelperCtx* pCtx);

int UdpHelperSend(UdpHelperCtx* pCtx, const char* toIP, unsigned short toPort,
				  const char* data, int len);

int UdpHelperClose(UdpHelperCtx* pCtx);

int UdpHelperRelease(UdpHelperCtx* pCtx);

#endif//DATAPROXY_UDPHELPER_H

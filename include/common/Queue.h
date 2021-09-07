//
// Created by hj on 2021/9/3.
//

#ifndef DATAPROXY_QUEUE_H
#define DATAPROXY_QUEUE_H

#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ElemDataFreeFunc)(void* p);

typedef struct {
	void* data;
	ElemDataFreeFunc free_func;
} QueueElem;

typedef struct {
	int front;
	int rear;
	int size;
	int capacity;
	QueueElem* buffer;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} Queue;

Queue* QueueCreate(int capacity);

int QueuePush(Queue* q, QueueElem* elem);

QueueElem* QueueFront(Queue* q, int waitTimeMs);

QueueElem* QueuePop(Queue* q, int waitTimeMs);

bool QueueIsEmpty(Queue* q);

int QueueSize(Queue* q);

int QueueClear(Queue* q);

int QueueDestroy(Queue* q);

#ifdef __cplusplus
}
#endif

#endif//DATAPROXY_QUEUE_H

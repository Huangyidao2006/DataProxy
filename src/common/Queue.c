//
// Created by hj on 2021/9/3.
//

#include "common/Queue.h"
#include "common/Error.h"

#include <stdlib.h>
#include <sys/time.h>

Queue* QueueCreate(int capacity) {
	if (capacity <= 0) {
		return NULL;
	}

	Queue* q = (Queue*) malloc (sizeof(Queue));
	q->front = 0;
	q->rear = 0;
	q->size = 0;
	q->capacity = capacity;
	q->buffer = (QueueElem*) malloc(sizeof(QueueElem) * (capacity + 1));

	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);

	return q;
}

int QueuePush(Queue* q, QueueElem* elem) {
	if (NULL == q) {
		return ERROR_NULL_PTR;
	}

	pthread_mutex_lock(&q->mutex);

	if ((q->rear + 1) % (q->capacity + 1) == q->front) {
		pthread_mutex_unlock(&q->mutex);

        return ERROR_QUEUE_FULL;
	}

    (q->buffer)[q->rear] = *elem;
	q->rear = (q->rear + 1) % (q->capacity + 1);

	pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);

	return SUCCESS;
}

QueueElem* QueueFront(Queue* q, int waitTimeMs) {
    if (NULL == q) {
        return NULL;
    }

    pthread_mutex_lock(&q->mutex);

    if (q->front == q->rear) {
		if (0 == waitTimeMs) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
		} else if (-1 == waitTimeMs) {
            pthread_cond_wait(&q->cond, &q->mutex);
		} else {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			struct timespec ts;
			ts.tv_sec = tv.tv_sec;
			ts.tv_nsec = tv.tv_usec * 1000 + waitTimeMs * 1000000;

			pthread_cond_timedwait(&q->cond, &q->mutex, &ts);
		}
	}

	QueueElem* elem;

	if (q->front == q->rear) {
        elem = NULL;
	} else {
        elem = &(q->buffer)[q->front];
	}

    pthread_mutex_unlock(&q->mutex);

    return elem;
}

QueueElem* QueuePop(Queue* q, int waitTimeMs) {
    if (NULL == q) {
        return NULL;
    }

    pthread_mutex_lock(&q->mutex);

    if (q->front == q->rear) {
        if (0 == waitTimeMs) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        } else if (-1 == waitTimeMs) {
            pthread_cond_wait(&q->cond, &q->mutex);
        } else {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            struct timespec ts;
            ts.tv_sec = tv.tv_sec;
            ts.tv_nsec = tv.tv_usec * 1000 + waitTimeMs * 1000000;

            pthread_cond_timedwait(&q->cond, &q->mutex, &ts);
        }
    }

    QueueElem* elem;

    if (q->front == q->rear) {
        elem = NULL;
    } else {
        elem = &(q->buffer)[q->front];
		q->front = (q->front + 1) % (q->capacity + 1);
    }

    pthread_mutex_unlock(&q->mutex);

    return elem;
}

bool QueueIsEmpty(Queue* q) {
    if (NULL == q) {
        return true;
    }

    pthread_mutex_lock(&q->mutex);

	bool isEmpty = q->front == q->rear;

    pthread_mutex_unlock(&q->mutex);

	return isEmpty;
}

int QueueSize(Queue* q) {
    if (NULL == q) {
        return 0;
    }

    pthread_mutex_lock(&q->mutex);

    int size = (q->rear - q->front + q->capacity + 1) % (q->capacity + 1);

    pthread_mutex_unlock(&q->mutex);

	return size;
}

int QueueClear(Queue* q) {
    if (NULL == q) {
        return ERROR_NULL_PTR;
    }

    pthread_mutex_lock(&q->mutex);

	while (q->front != q->rear) {
		QueueElem elem = q->buffer[q->front];
		if (NULL != elem.free_func) {
			elem.free_func(elem.data);
		}

        q->front = (q->front + 1) % (q->capacity + 1);
	}

    pthread_mutex_unlock(&q->mutex);

    return SUCCESS;
}

int QueueDestroy(Queue* q) {
    if (NULL == q) {
        return ERROR_NULL_PTR;
    }

    while (!QueueIsEmpty(q)) {
		QueueElem* elem = QueuePop(q, 0);
		if (NULL != elem->free_func) {
			elem->free_func(elem->data);
		}
	}

	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);

	free(q);

	return SUCCESS;
}

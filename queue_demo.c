//
// Created by hj on 2021/9/3.
//

#include "log/Log.h"
#include "common/Error.h"
#include "common/Common.h"
#include "common/Queue.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
	int val;
} MyData;

void FreeMyData(MyData* data) {
	free(data);
}

int main(int argc, char* argv[]) {
    Queue* q = QueueCreate(100);

	for (int i = 0; i < 110; i++) {
		MyData* data = (MyData*) malloc(sizeof(MyData));
		data->val = i;
		QueueElem e;
		e.data = data;
		e.free_func = FreeMyData;

		int ret = QueuePush(q, &e);
		int size = QueueSize(q);

		LOG_D("QueuePush, ret=%d, size=%d", ret, size);
	}

	QueueElem* elem = QueuePop(q, 0);
	QueueClear(q);

	LOG_D("elem, val=%d", ((MyData*) (elem->data))->val);
	LOG_D("size=%d", QueueSize(q));

	QueueDestroy(q);

	return 0;
}
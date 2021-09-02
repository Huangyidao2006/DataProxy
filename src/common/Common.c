//
// Created by hj on 2021/9/2.
//

#include "common/Common.h"

#include <unistd.h>

void sleepMs(int timeMs) {
	usleep(timeMs * 1000);
}

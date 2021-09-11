//
// Created by hj on 2021/9/11.
//

#include "handler/MessageHandler.h"
#include "time/Time.h"
#include "common/Common.h"
#include "log/Log.h"

#include <stdlib.h>
#include <string.h>

const int MSG_BEGIN = 1;
const int MSG_STOP = 2;
const int MSG_TIMEOUT = 3;

class Object {
public:
	int val;
};

class MyHandler : public MessageHandler<Object> {
protected:
	void handleMessage(int what, std::shared_ptr<Object> &data) override {
		switch (what) {
			case MSG_BEGIN: {
                LOG_D("MSG_BEGIN, val=%d", data->val);
			} break;

			case MSG_STOP: {
                LOG_D("MSG_STOP, val=%d", data->val);
			} break;

			case MSG_TIMEOUT: {
                LOG_D("MSG_TIMEOUT, val=%d", data->val);
			}

			default:
				;
		}
	}
};

int main(int argc, char* argv[]) {
	std::shared_ptr<MyHandler> myHandler = std::make_shared<MyHandler>();
	myHandler->start();

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        int len = strlen(buffer);
        buffer[len - 1] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            break;
        } else if (strcmp(buffer, "s") == 0) {
			std::shared_ptr<Object> obj = std::make_shared<Object>();
			obj->val = 123;
            myHandler->sendMessage(MSG_BEGIN, obj);
		} else if (strcmp(buffer, "st") == 0) {
            std::shared_ptr<Object> obj = std::make_shared<Object>();
            obj->val = 456;
            myHandler->sendMessage(MSG_BEGIN, obj);
		} else if (strcmp(buffer, "t") == 0) {
			LOG_D("send MSG_TIMEOUT");

            std::shared_ptr<Object> obj = std::make_shared<Object>();
            obj->val = 789;
            myHandler->sendMessage(MSG_TIMEOUT, obj, 2000);
		} else if (strcmp(buffer, "rt") == 0) {
			myHandler->removeMessages(MSG_TIMEOUT);
		} else {
			printf("unknown cmd: %s\n", buffer);
		}
    }

	myHandler->stop();
	myHandler->clearMessages();

	return 0;
}

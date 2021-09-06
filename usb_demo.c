/**
 * Created by huangjian on 2021/9/6.
 */

#include "common/Error.h"
#include "log/Log.h"
#include "usb/UsbManager.h"

#include <stdlib.h>

UsbManager g_UsbManager;

void UsbManagerRecvCb(const char* data, int len) {
	char* buffer = (char*) malloc(len + 1);
	memcpy(buffer, data, len);
	buffer[len] = '\0';

	LOG_D("recv, content=%s, len=%d", buffer, len);

	free(buffer);

	char* reply = "data received";
	UsbManagerSend(&g_UsbManager, reply, strlen(reply));
}

void UsbManagerErrorCb(int error, const char* des) {
	LOG_D("error=%d, %s", error, des);
}

int main(int argc, char* argv[]) {
	g_UsbManager.recv_cb = UsbManagerRecvCb;
	g_UsbManager.error_cb = UsbManagerErrorCb;

	int ret = UsbManagerInit(&g_UsbManager);
	if (SUCCESS != ret) {
		LOG_E("init usb error");
		return 1;
	}

	char buffer[1024];
	while (true) {
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, sizeof(buffer), stdin);
		int len = strlen(buffer);
		buffer[len - 1] = '\0';

		if (strcmp(buffer, "exit") == 0) {
			break;
		}
	}

	LOG_D("try to exit");

	UsbManagerDestroy(&g_UsbManager);

	sleepMs(3000);

	return 0;
}

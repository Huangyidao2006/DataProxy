/**
 * Created by huangjian on 2021/9/6.
 */

#ifndef DATAPROXY_USBMANAGER_H
#define DATAPROXY_USBMANAGER_H

#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/Common.h"
#include "common/Error.h"
#include "log/Log.h"

#define EP_IN 0x81
#define EP_OUT 0x01

#define GOOGLE_VID 0x18d1
#define ACCESSORY_PID 0x2d01     /* accessory with adb */
#define ACCESSORY_PID_ALT 0x2d00 /* accessory without adb */

/*64 bytes for USB full-speed accessories
512 bytes for USB high-speed accessories
The Android accessory protocol supports packet buffers up to 16384 bytes*/
#define AOA_BUFF_MAX 16384
#define LEN 2

#define INDEX_MANUFACTURER 0
#define INDEX_MODEL_NAME 1
#define INDEX_DESCRIPTION 2
#define INDEX_VERSION 3
#define INDEX_URI 4
#define INDEX_SERIAL_NUMBER 5

struct UsbAccessory {
	char* manufacturer;
	char* modelName;
	char* description;
	char* version;
	char* URI;
	char* serialNumber;
};

typedef void (*UsbManagerRecvCbFunc)(const char* data, int len);
typedef void (*UsbManagerErrorCbFunc)(int error, const char* des);

typedef struct {
	char* recv_buffer;
	int recv_buffer_len;
	libusb_device_handle* device_handle;
	UsbManagerRecvCbFunc recv_cb;
	UsbManagerErrorCbFunc error_cb;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} UsbManager;

int UsbManagerInit(UsbManager* pInst);

int UsbManagerSend(UsbManager* pInst, char* data, int len);

int UsbManagerDestroy(UsbManager* pInst);

#endif//DATAPROXY_USBMANAGER_H

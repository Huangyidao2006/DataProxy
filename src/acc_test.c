/**
 *
 * filename: usbacc.c
 * brief: Use libusb to emulate usb android open accesory
 *
 */

#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define EP_IN 0x81
#define EP_OUT 0x01

/* Android device vendor/product */
#define AD_VID 0x2717
#define PID 0xff48

#define GOOGLE_VID 0x18d1
#define ACCESSORY_PID 0x2d01 /* accessory with adb */
#define ACCESSORY_PID_ALT 0x2d00 /* accessory without adb */

/*64 bytes for USB full-speed accessories
512 bytes for USB high-speed accessories
The Android accessory protocol supports packet buffers up to 16384 bytes*/
#define AOA_BUFF_MAX 16384
#define LEN 2

int init(void);
int deInit(void);
int setupAccessory(void);
int usbSendCtrl(char *buff, int req, int index);
void error(int code);
void status(int code);
void *usbRWHdlr(void * threadarg);

struct libusb_device_handle* handle;
char stop;
char success = 0;

struct usbAccessory {
	char* manufacturer;
	char* modelName;
	char* version;
};

struct usbAccessory gadgetAccessory = {
		"Lutixia",
		"Demo",
		"1.0",
};

int main(int argc, char *argv[])
{
	pthread_t tid;
	if (init() < 0)
		return -1;

	if (setupAccessory() < 0) {
		fprintf(stdout, "Error setting up accessory\n");
		deInit();
		return -1;
	}
	pthread_create(&tid, NULL, usbRWHdlr, NULL);
	pthread_join(tid, NULL);

	deInit();
	fprintf(stdout, "Done, no errors\n");
	return 0;
}

void *usbRWHdlr(void * threadarg)
{
	int response, transferred;
	unsigned int index = 0;
	unsigned char inBuff[AOA_BUFF_MAX] = {0};
	unsigned char outBuff[AOA_BUFF_MAX] = {0};

	for (;;) {
		response =
				libusb_bulk_transfer(handle, EP_IN, inBuff, sizeof(inBuff), &transferred, 0);
		if (response < 0) {
			error(response);
			return NULL;
		}
		fprintf(stdout, "msg: %s\n", inBuff);
		sprintf(outBuff, "ACK: %07d", index++);
		response =
				libusb_bulk_transfer(handle, EP_OUT, outBuff, strlen(outBuff), &transferred, 0);
		if (response < 0) {
			error(response);
			return NULL;
		}
		sleep(1);
	}
}

int init()
{
	libusb_init(NULL);
	if ((handle = libusb_open_device_with_vid_pid(NULL, AD_VID, PID)) == NULL) {
		fprintf(stdout, "Problem acquireing handle\n");
		return -1;
	}
	libusb_claim_interface(handle, 0);
	return 0;
}

int deInit()
{
	if (handle != NULL) {
		libusb_release_interface(handle, 0);
		libusb_close(handle);
	}
	libusb_exit(NULL);
	return 0;
}

int usbSendCtrl(char *buff, int req, int index)
{
	int response = 0;

	if (NULL != buff) {
		response =
				libusb_control_transfer(handle, 0x40, req, 0, index, buff, strlen(buff) + 1 , 0);
	} else {
		response =
				libusb_control_transfer(handle, 0x40, req, 0, index, buff, 0, 0);
	}

	if (response < 0) {
		error(response);
		return -1;
	}
}

int setupAccessory()
{
	unsigned char ioBuffer[2];
	int devVersion;
	int response;
	int tries = 5;

	response = libusb_control_transfer(
			handle, //handle
			0xC0, //bmRequestType
			51, //bRequest
			0, //wValue
			0, //wIndex
			ioBuffer, //data
			2, //wLength
			0 //timeout
	);

	if (response < 0) {
		error(response);
		return-1;
	}

	devVersion = ioBuffer[1] << 8 | ioBuffer[0];
	fprintf(stdout,"Verion Code Device: %d \n", devVersion);

	usleep(1000);//sometimes hangs on the next transfer :(

	if (usbSendCtrl(gadgetAccessory.manufacturer, 52, 0) < 0) {
		return -1;
	}
	if (usbSendCtrl(gadgetAccessory.modelName, 52, 1) < 0) {
		return -1;
	}
	if (usbSendCtrl(gadgetAccessory.version, 52, 3) < 0) {
		return -1;
	}

	fprintf(stdout,"Accessory Identification sent\n");

	if (usbSendCtrl(NULL, 53, 0) < 0) {
		return -1;
	}

	fprintf(stdout,"Attempted to put device into accessory mode\n");

	if (handle != NULL) {
		libusb_release_interface (handle, 0);
	}

	for (;;) {
		tries--;
		if ((handle = libusb_open_device_with_vid_pid(NULL,
													  GOOGLE_VID, ACCESSORY_PID)) == NULL) {
			if (tries < 0) {
				return -1;
			}
		} else {
			break;
		}
		sleep(1);
	}

	libusb_claim_interface(handle, 0);
	fprintf(stdout, "Interface claimed, ready to transfer data\n");
	return 0;
}


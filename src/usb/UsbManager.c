/**
 * Created by huangjian on 2021/9/6.
 */

#include "usb/UsbManager.h"

#include <stdlib.h>

struct UsbAccessory g_GadgetInfo = {
	"MyCom",
	"DataProxy",
	"App proxy",
	"1.0",
	"",
	"abc123"
};

static libusb_context* g_pUsbCtx = NULL;
static volatile bool g_isStopMonitor = false;
static volatile bool g_isStopRecv = false;
static volatile bool g_isRecvStarted = false;

static int usbSendCtrl(libusb_device_handle* pHandle, char* buff, int req, int index);
static int setupAccessory(libusb_device_handle* pHandle);

static void* UsbMonitorThreadFunc(void* params);
static void* UsbRecvThreadFunc(void* params);

int UsbPlugCallback(libusb_context* ctx, libusb_device* device, libusb_hotplug_event event, void* user_data) {
	UsbManager* pInst = (UsbManager*) user_data;

	struct libusb_device_descriptor descriptor;
	libusb_get_device_descriptor(device, &descriptor);

	switch (event) {
		case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED: {
			LOG_I("device ARRIVED, vid=%x, pid=%x", descriptor.idVendor, descriptor.idProduct);

			if (descriptor.idVendor == GOOGLE_VID) {
				pthread_mutex_lock(&pInst->mutex);

				if (NULL != pInst->device_handle) {
					LOG_I("accessory already connected, ignore current device");
					pthread_mutex_unlock(&pInst->mutex);
					break;
				}

				pthread_mutex_unlock(&pInst->mutex);

				libusb_device_handle* pHandle = NULL;

				int tryCount = 5;
				for (;;) {
					tryCount--;
					if ((pHandle = libusb_open_device_with_vid_pid(g_pUsbCtx,
																   GOOGLE_VID, ACCESSORY_PID_ALT)) != NULL
						|| (pHandle = libusb_open_device_with_vid_pid(g_pUsbCtx,
																	  GOOGLE_VID, ACCESSORY_PID)) != NULL) {
						LOG_D("open accessory device successfully");
						break;
					} else {
						if (tryCount < 0) {
							break;
						}
					}
				}

				if (NULL == pHandle) {
					LOG_E("open accessory device failed");
					break;
				}

				int ret = libusb_claim_interface(pHandle, 0);
				if (SUCCESS == ret) {
					LOG_I("accessory interface claimed, ready to transfer data");
				} else {
					LOG_E("libusb_claim_interface, ret=%d, %s", ret, libusb_error_name(ret));
					break;
				}

				pthread_mutex_lock(&pInst->mutex);

				g_isStopRecv = false;
				pInst->device_handle = pHandle;

				pthread_mutex_unlock(&pInst->mutex);

				pthread_t tid;
				pthread_create(&tid, NULL, UsbRecvThreadFunc, pInst);
			} else {
				// try to open the arrived device
				libusb_device_handle* pHandle = libusb_open_device_with_vid_pid(g_pUsbCtx, descriptor.idVendor, descriptor.idProduct);
				if (NULL == pHandle) {
					LOG_E("open device failed");
				} else {
					LOG_D("open device successfully");

					int ret = libusb_claim_interface(pHandle, 0);
					if (SUCCESS != ret) {
						LOG_E("libusb_claim_interface, ret=%d, %s", ret, libusb_error_name(ret));
					} else {
						ret = setupAccessory(pHandle);
						if (SUCCESS != ret) {
							LOG_E("setupAccessory failed");
						}
					}
				}
			}
		} break;

		case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT: {
			LOG_I("device LEFT, vid=%x, pid=%x", descriptor.idVendor, descriptor.idProduct);

			if (descriptor.idVendor == GOOGLE_VID &&
				(descriptor.idProduct == ACCESSORY_PID || descriptor.idProduct == ACCESSORY_PID_ALT)) {
				g_isStopRecv = true;
			}
		} break;

		default:;
	}

	return 0;
}

static void* UsbMonitorThreadFunc(void* params) {
	LOG_I("usb monitor started");

	libusb_hotplug_callback_handle cb_handle;
	libusb_hotplug_register_callback(g_pUsbCtx,
									 LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
									 LIBUSB_HOTPLUG_NO_FLAGS,
									 LIBUSB_HOTPLUG_MATCH_ANY,
									 LIBUSB_HOTPLUG_MATCH_ANY,
									 LIBUSB_HOTPLUG_MATCH_ANY,
									 UsbPlugCallback,
									 params,
									 &cb_handle);

	while (!g_isStopMonitor) {
		struct timeval tv = {1, 0};
//		libusb_lock_events(g_pUsbCtx);
//
		int completed = 0;
		int ret = libusb_handle_events_timeout_completed(g_pUsbCtx, &tv, &completed);
//
//		libusb_unlock_events(g_pUsbCtx);
//		int ret = libusb_handle_events(g_pUsbCtx);

		if (ret < 0) {
			LOG_D("handle events failed: %s", libusb_error_name(ret));
			break;
		}
	}

	LOG_I("usb monitor stopped\n");

	libusb_hotplug_deregister_callback(g_pUsbCtx, 0);

	return NULL;
}

static void* UsbRecvThreadFunc(void* params) {
	LOG_I("receiving started");

	g_isRecvStarted = true;

	UsbManager* pInst = (UsbManager*) params;
	struct libusb_device_handle* pHandle = pInst->device_handle;

	int ret, transferred;
	while (!g_isStopRecv) {
		ret = libusb_bulk_transfer(pHandle, EP_IN, (unsigned char*) pInst->recv_buffer,
								   pInst->recv_buffer_len, &transferred, 0);
		if (ret < 0) {
			if (NULL != pInst->error_cb) {
				pInst->error_cb(ERROR_USB_RECV, "usb recv error");
			}

			goto error;
		} else {
			pInst->recv_cb(pInst->recv_buffer, transferred);
		}
	}

error:
	pthread_mutex_lock(&pInst->mutex);

	g_isStopRecv = true;
	g_isRecvStarted = false;

	libusb_close(pHandle);
	pInst->device_handle = NULL;

	pthread_cond_signal(&pInst->cond);

	pthread_mutex_unlock(&pInst->mutex);

	LOG_I("receiving stopped");

	return NULL;
}

static int setupAccessory(libusb_device_handle* pHandle) {
	unsigned char ioBuffer[2];
	int devVersion;
	int ret;

	ret = libusb_control_transfer(
		pHandle, //handle
		0xC0,    //bmRequestType
		51,      //bRequest
		0,       //wValue
		0,       //wIndex
		ioBuffer,//data
		2,       //wLength
		0        //timeout
	);

	if (ret < 0) {
		LOG_E("libusb_control_transfer, ret=%d, %s", ret, libusb_error_name(ret));
		return ERROR_FAILED;
	}

	devVersion = ioBuffer[1] << 8 | ioBuffer[0];
	usleep(1000);//sometimes hangs on the next transfer :(

	if (usbSendCtrl(pHandle, g_GadgetInfo.manufacturer, 52, INDEX_MANUFACTURER) < 0) {
		return ERROR_FAILED;
	}

	if (usbSendCtrl(pHandle, g_GadgetInfo.modelName, 52, INDEX_MODEL_NAME) < 0) {
		return ERROR_FAILED;
	}

	if (usbSendCtrl(pHandle, g_GadgetInfo.description, 52, INDEX_DESCRIPTION) < 0) {
		return ERROR_FAILED;
	}

	if (usbSendCtrl(pHandle, g_GadgetInfo.version, 52, INDEX_VERSION) < 0) {
		return ERROR_FAILED;
	}

	if (usbSendCtrl(pHandle, g_GadgetInfo.URI, 52, INDEX_URI) < 0) {
		return ERROR_FAILED;
	}

	if (usbSendCtrl(pHandle, g_GadgetInfo.serialNumber, 52, INDEX_SERIAL_NUMBER) < 0) {
		return ERROR_FAILED;
	}

	LOG_D("accessory info sent");

	if (usbSendCtrl(pHandle, NULL, 53, 0) < 0) {
		return ERROR_FAILED;
	}

	LOG_D("try to transfer device into accessory mode");

	if (pHandle != NULL) {
		libusb_release_interface(pHandle, 0);
	}

	return SUCCESS;
}

static int usbSendCtrl(libusb_device_handle* pHandle, char* buff, int req, int index) {
	int ret = 0;

	if (NULL != buff) {
		ret =
			libusb_control_transfer(pHandle, 0x40, req, 0, index, buff, strlen(buff) + 1, 0);
	} else {
		ret =
			libusb_control_transfer(pHandle, 0x40, req, 0, index, buff, 0, 0);
	}

	if (ret < 0) {
		LOG_E("error=%s", libusb_error_name(ret));
		return ERROR_FAILED;
	}
}

int UsbManagerInit(UsbManager* pInst) {
	if (NULL == pInst) {
		return ERROR_NULL_PTR;
	}

	int ret = 0;
	if (NULL == g_pUsbCtx) {
		ret = libusb_init(&g_pUsbCtx);
		if (SUCCESS != ret) {
			LOG_D("libusb_init, error=%d", ret);
			return ERROR_FAILED;
		}
	}

	if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		LOG_D("hotplug is not supported");
		return ERROR_FAILED;
	}

	pInst->recv_buffer = (char*) malloc(AOA_BUFF_MAX);
	pInst->recv_buffer_len = AOA_BUFF_MAX;
	pInst->device_handle = NULL;

	pthread_mutex_init(&pInst->mutex, NULL);
	pthread_cond_init(&pInst->cond, NULL);

	g_isStopMonitor = false;
	pthread_t tid;
	pthread_create(&tid, NULL, UsbMonitorThreadFunc, pInst);

	return SUCCESS;
}

int UsbManagerSend(UsbManager* pInst, char* data, int len) {
	if (NULL == pInst) {
		return ERROR_NULL_PTR;
	}

	int ret, transferred;
	ret = libusb_bulk_transfer(pInst->device_handle, EP_OUT, (unsigned char*) data, len, &transferred, 0);
	if (SUCCESS != ret) {
		LOG_E("send data, ret=%d, %s", ret, libusb_error_name(ret));

		return ERROR_FAILED;
	}

	return transferred;
}

int UsbManagerDestroy(UsbManager* pInst) {
	if (NULL == pInst) {
		return ERROR_NULL_PTR;
	}

	pthread_mutex_lock(&pInst->mutex);

	g_isStopMonitor = true;
	g_isStopRecv = true;

	if (g_isRecvStarted) {
		pthread_cond_wait(&pInst->cond, &pInst->mutex);
	}

	pthread_mutex_unlock(&pInst->mutex);

	pthread_mutex_destroy(&pInst->mutex);
	pthread_cond_destroy(&pInst->cond);

	free(pInst->recv_buffer);

	return SUCCESS;
}

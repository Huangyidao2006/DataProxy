//
// Created by hj on 2021/11/27.
//

#include "usbmuxd/iOSUsbMuxManager.h"
#include "log/Log.h"
#include "common/Error.h"

void usbMuxEventCb(const usbmuxd_event_t *event, void *user_data) {
	if (user_data) {
		auto pManager = (iOSUsbMuxManager*) user_data;
		pManager->processUsbMuxEvent(event);
	}
}

iOSUsbMuxManager::iOSUsbMuxManager(int remotePort, const std::shared_ptr<iOSUsbMuxListener>& listener) :
    mRemotePort(remotePort), mIsConnected(false) {
    m_pRecvBuffer = new char[MUX_RECV_BUFFER_SIZE];
    mUsbMuxListener = listener;
    mUsbMuxCtx = nullptr;
    mFd = -1;
}

iOSUsbMuxManager::~iOSUsbMuxManager() {
    usbmuxd_events_unsubscribe(mUsbMuxCtx);

	delete m_pRecvBuffer;
}

int iOSUsbMuxManager::init() {
    int ret = usbmuxd_events_subscribe(&mUsbMuxCtx, usbMuxEventCb, this);
	if (SUCCESS != ret) {
		LOG_E("usbmuxd_events_subscribe, ret=%d", ret);
	}

	return SUCCESS;
}

bool iOSUsbMuxManager::isConnected() {
	return mIsConnected;
}

int iOSUsbMuxManager::send(const char* data, int len) {
	if (!mIsConnected) {
		LOG_E("iOS device has not been connected yet");
		return ERROR_USB_SEND;
	}

	int offset = 0;
	while (offset != len) {
        uint32_t sent;
        int ret = usbmuxd_send(mFd, data + offset, len - offset, &sent);
		if (SUCCESS != ret) {
			return ERROR_SOCK_SEND;
		}

		offset += sent;
	}

	return SUCCESS;
}

void iOSUsbMuxManager::processUsbMuxEvent(const usbmuxd_event_t* event) {
	switch (event->event) {
		case UE_DEVICE_ADD: {
            mCurDevInfo = event->device;

			LOG_I("iOSUsbMux device added, handle=%ud, product_id=%ud, udid=%s, conn_type=%d, conn_data=%s",
				  mCurDevInfo.handle, mCurDevInfo.product_id, mCurDevInfo.udid, mCurDevInfo.conn_type, mCurDevInfo.conn_data);

			LOG_I("try to connect to iOS device");

            mFd = usbmuxd_connect(mCurDevInfo.handle, mRemotePort);
			if (mFd < 0) {
				LOG_E("usbmuxd_connect failed, fd=%d", mFd);
				return;
			}

            LOG_I("connect to iOS device success");

            mIsConnected = true;
            mRecvThread = std::make_shared<std::thread>(&iOSUsbMuxManager::recvThreadFunc, this);
		} break;

		case UE_DEVICE_REMOVE: {
            mIsConnected = false;
		} break;

		case UE_DEVICE_PAIRED: {

		} break;

		default:;
	}
}

void iOSUsbMuxManager::recvThreadFunc() {
	LOG_I("UsbMux receive thread started");

    while (mIsConnected) {
		uint32_t recvSize;
        int ret = usbmuxd_recv(mFd, m_pRecvBuffer, MUX_RECV_BUFFER_SIZE, &recvSize);
		if (SUCCESS != ret) {
			LOG_E("usbmuxd_recv, ret=%d", ret);

            mIsConnected = false;

			if (mUsbMuxListener != nullptr) {
				mUsbMuxListener->onError(ERROR_USB_RECV, "recv from usbmuxd failed");
			}
		} else {
			if (mUsbMuxListener) {
				mUsbMuxListener->onRecv(m_pRecvBuffer, recvSize);
			}
		}
	}

	if (mRecvThread != nullptr && mRecvThread->joinable()) {
		mRecvThread->detach();
	}

    LOG_I("UsbMux receive thread stopped");
}

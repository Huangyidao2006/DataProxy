//
// Created by hj on 2021/11/27.
//

#ifndef DATAPROXY_IOSUSBMUXMANAGER_H
#define DATAPROXY_IOSUSBMUXMANAGER_H

#include "usbmuxd.h"

#include <string>
#include <memory>
#include <thread>

#define MUX_RECV_BUFFER_SIZE (100 * 1024)

class iOSUsbMuxListener {
public:
	virtual void onRecv(const char* data, int len) = 0;
	virtual void onError(int error, const char* des) = 0;
};

class iOSUsbMuxManager {
private:
	char* m_pRecvBuffer;

	int mRemotePort;

	bool mIsConnected;

	std::shared_ptr<iOSUsbMuxListener> mUsbMuxListener;

    usbmuxd_subscription_context_t mUsbMuxCtx;

    usbmuxd_device_info_t mCurDevInfo{};

	int mFd;

	std::shared_ptr<std::thread> mRecvThread;

public:
	iOSUsbMuxManager(int remotePort, const std::shared_ptr<iOSUsbMuxListener>& listener);

	~iOSUsbMuxManager();

	int init();

	bool isConnected();

	int send(const char* data, int len);

private:
	void processUsbMuxEvent(const usbmuxd_event_t *event);

	void recvThreadFunc();

	friend void usbMuxEventCb(const usbmuxd_event_t *event, void *user_data);
	
	friend std::thread;
};

#endif//DATAPROXY_IOSUSBMUXMANAGER_H

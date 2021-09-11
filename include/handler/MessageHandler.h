/**
 * Created by huangjian on 2021/9/7.
 */

#ifndef DATAPROXY_MESSAGEHANDLER_H
#define DATAPROXY_MESSAGEHANDLER_H

#include "time/Time.h"

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

template<typename T>
class MessageHandler : public std::enable_shared_from_this<MessageHandler<T>> {
public:
	class Message {
	public:
		int what;
		std::shared_ptr<T> data;

	private:
		long long time;
        std::shared_ptr<Message> next;

		friend class MessageHandler<T>;
	};

protected:
//	std::queue<std::shared_ptr<Message>> mQueue;

	std::shared_ptr<Message> mMessageList = nullptr;

	std::mutex mMutex;

	std::condition_variable mCond;

	std::shared_ptr<std::thread> m_pThread;

	bool mIsStop = false;

public:
	MessageHandler();

	virtual ~MessageHandler();

	bool start();

	void stop();

	void sendMessage(int what, std::shared_ptr<T>& data, int delayMillis = 0);

    void sendEmptyMessage(int what, int delayMillis = 0);

    void removeMessages(int what);

	void clearMessages();

protected:
	virtual void handleMessage(int what, std::shared_ptr<T>& data);

private:
	void thread_func();

	friend class std::thread;
};

template<typename T>
MessageHandler<T>::MessageHandler() {
    mMessageList = std::make_shared<Message>();
	mMessageList->next = nullptr;
}

template<typename T>
MessageHandler<T>::~MessageHandler() {
	if (m_pThread != nullptr && m_pThread->joinable()) {
		m_pThread->detach();
	}
}

template<typename T>
bool MessageHandler<T>::start() {
	if (m_pThread != nullptr) {
		return false;
	}

	std::unique_lock<std::mutex> lock(mMutex);

	m_pThread = std::make_shared<std::thread>(&MessageHandler<T>::thread_func,
											  this);

	mCond.wait(lock);

	return true;
}

template<typename T>
void MessageHandler<T>::stop() {
	if (mIsStop) {
		return;
	}

	mIsStop = true;

	std::unique_lock<std::mutex> lock(mMutex);

	mCond.wait(lock);
}

template<typename T>
void MessageHandler<T>::thread_func() {
	auto ref = this->shared_from_this();

	{
		std::unique_lock<std::mutex> lock(mMutex);

		mCond.notify_all();
	}

	while (!mIsStop) {
		std::shared_ptr<Message> msg = nullptr;

		{
			std::unique_lock<std::mutex> lock(mMutex);

//			if (mQueue.empty()) {
//				auto duration = std::chrono::milliseconds(500);
//				mCond.wait_for(lock, duration);
//			} else {
//				msg = mQueue.front();
//				mQueue.pop();
//			}

			auto now = bootTimeMills();

			auto cur = mMessageList->next;
			if (cur != nullptr) {
                if (cur->time <= now) {
					msg = cur;
					mMessageList->next = cur->next;
				} else {
                    auto waitTimeMills = cur->time - now;
					auto duration = std::chrono::milliseconds(waitTimeMills);

                    mCond.wait_for(lock, duration);
				}
			} else {
				auto duration = std::chrono::milliseconds(1000);
				mCond.wait_for(lock, duration);
			}
		}

		if (msg != nullptr) {
			handleMessage(msg->what, msg->data);
		}
	}

	if (m_pThread->joinable()) {
		m_pThread->detach();
	}

	{
		std::unique_lock<std::mutex> lock(mMutex);

		mCond.notify_all();
	}
}

//template<typename T>
//void MessageHandler<T>::sendMessage(int what, std::shared_ptr<T>& data, int delayMillis) {
//	std::unique_lock<std::mutex> lock(mMutex);
//
//	auto msg = std::make_shared<Message>();
//	msg->what = what;
//	msg->data = data;
//
//	mQueue.push(msg);
//
//	mCond.notify_all();
//}

template<typename T>
void MessageHandler<T>::sendMessage(int what, std::shared_ptr<T>& data, int delayMillis) {
    std::unique_lock<std::mutex> lock(mMutex);

    auto msg = std::make_shared<Message>();
    msg->what = what;
    msg->data = data;
	msg->time = bootTimeMills() + delayMillis;
	msg->next = nullptr;

	if (mMessageList->next == nullptr) {
        mMessageList->next = msg;
	} else {
		// 按时间顺序插入到list中
		auto cur = mMessageList;

        // 找到插入位置插入
        auto curNext = cur->next;
        while (curNext != nullptr && msg->time >= curNext->time) {
            cur = curNext;
            curNext = curNext->next;
        }

        cur->next = msg;
        msg->next = curNext;
	}

    mCond.notify_all();
}

template<typename T>
void MessageHandler<T>::sendEmptyMessage(int what, int delayMillis) {
	std::shared_ptr<T> data;
	sendMessage(what, data, delayMillis);
}

//template<typename T>
//void MessageHandler<T>::clearMessage() {
//	std::unique_lock<std::mutex> lock(mMutex);
//
//	while (!mQueue.empty()) {
//		mQueue.pop();
//	}
//}

template<typename T>
void MessageHandler<T>::clearMessages() {
    std::unique_lock<std::mutex> lock(mMutex);

    mMessageList->next = nullptr;
}

template<typename T>
void MessageHandler<T>::removeMessages(int what) {
    std::unique_lock<std::mutex> lock(mMutex);

	auto cur = mMessageList;
	auto curNext = cur->next;

	while (curNext != nullptr) {
		if (curNext->what == what) {
			cur->next = curNext->next;
		} else {
			cur = curNext;
		}

        curNext = cur->next;
	}
}


template<typename T>
void MessageHandler<T>::handleMessage(int what, std::shared_ptr<T>& data) {

}

#endif//DATAPROXY_MESSAGEHANDLER_H

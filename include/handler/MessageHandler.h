/**
 * Created by huangjian on 2021/9/7.
 */

#ifndef DATAPROXY_MESSAGEHANDLER_H
#define DATAPROXY_MESSAGEHANDLER_H

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

template<typename T>
class MessageHandler : public std::enable_shared_from_this<MessageHandler<T>> {
public:
	class MessageEntity {
	public:
		int what;
		std::shared_ptr<T> msg;
	};

protected:
	std::queue<std::shared_ptr<MessageEntity>> mQueue;

	std::mutex mMutex;

	std::condition_variable mCond;

	std::shared_ptr<std::thread> m_pThread;

	bool mIsStop = false;

public:
	MessageHandler();

	virtual ~MessageHandler();

	bool start();

	void stop();

	void sendMessage(int what, std::shared_ptr<T>& msg);

	void clearMessage();

protected:
	virtual void handleMessage(int what, std::shared_ptr<T>& msg);

private:
	void thread_func();

	friend class std::thread;
};

template<typename T>
MessageHandler<T>::MessageHandler() {

}

template<typename T>
MessageHandler<T>::~MessageHandler() {
	if (m_pThread->joinable()) {
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
		std::shared_ptr<MessageEntity> entity = nullptr;

		{
			std::unique_lock<std::mutex> lock(mMutex);

			if (mQueue.empty()) {
				auto duration = std::chrono::milliseconds(500);
				mCond.wait_for(lock, duration);
			} else {
				entity = mQueue.front();
				mQueue.pop();
			}
		}

		if (entity != nullptr) {
			handleMessage(entity->what, entity->msg);
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

template<typename T>
void MessageHandler<T>::sendMessage(int what, std::shared_ptr<T>& msg) {
	std::unique_lock<std::mutex> lock(mMutex);

	auto entity = std::make_shared<MessageEntity>();
	entity->what = what;
	entity->msg = msg;

	mQueue.push(entity);

	mCond.notify_all();
}

template<typename T>
void MessageHandler<T>::clearMessage() {
	std::unique_lock<std::mutex> lock(mMutex);

	while (!mQueue.empty()) {
		mQueue.pop();
	}
}

template<typename T>
void MessageHandler<T>::handleMessage(int what, std::shared_ptr<T>& msg) {

}

#endif//DATAPROXY_MESSAGEHANDLER_H

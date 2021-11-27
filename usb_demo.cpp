/**
 * Created by huangjian on 2021/9/6.
 */

#include "common/Error.h"
#include "log/Log.h"

#include "usb/UsbManager.h"
#include "net/TcpClient.h"
#include "net/UdpHelper.h"
#include "src/proto/proxy.pb.h"
#include "handler/MessageHandler.h"

#include <stdlib.h>
#include <arpa/inet.h>

#include <string>
#include <memory>
#include <map>

using namespace std;

#define FRAME_BEGIN "#frame-begin#"
#define FRAME_HEADER_LEN (strlen(FRAME_BEGIN) + sizeof(uint32_t))

#define MSG_PROCESS_PROXY_MESSAGE 1
#define MSG_CLOSE_ALL_CONN 2
#define MSG_HEART_BEAT_TIMEOUT 3

class ProxyMsgHandler;

UsbManager g_UsbManager;
shared_ptr<ProxyMsgHandler> g_pProxyMsgHandler;

string g_UsbFrameBuffer;

string readUsbFrame() {
	if (g_UsbFrameBuffer.length() < FRAME_HEADER_LEN) {
		return "";
	}

	uint32_t frameDataLen = 0;
	if (memcmp(g_UsbFrameBuffer.c_str(), FRAME_BEGIN, strlen(FRAME_BEGIN)) == 0) {
		frameDataLen = ntohl(*((uint32_t*) &g_UsbFrameBuffer[strlen(FRAME_BEGIN)]));

		if (FRAME_HEADER_LEN + frameDataLen <= g_UsbFrameBuffer.length()) {
			string frameData(&g_UsbFrameBuffer[FRAME_HEADER_LEN], frameDataLen);
			g_UsbFrameBuffer = g_UsbFrameBuffer.substr(FRAME_HEADER_LEN + frameDataLen);

			return frameData;
		}
	}

	return "";
}

void processUsbFrame(const string& frameData);
void sendEmptyMessage(int msgType);

void UsbManagerRecvCb(const char* data, int len) {
	LOG_D("UsbManagerRecvCb, dataLen=%d", len);

	g_UsbFrameBuffer.append(data, len);

	while (true) {
		string frameData = readUsbFrame();
		if (frameData.empty()) {
			break;
		}

		processUsbFrame(frameData);
	}
}

void UsbManagerErrorCb(int error, const char* des) {
	LOG_E("usb error=%d, %s", error, des);

	// 断开所有网络连接
	sendEmptyMessage(MSG_CLOSE_ALL_CONN);
}

int sendToUsb(const char* data, int dataLen) {
	int offset = 0;
	while (offset != dataLen) {
		int sent = UsbManagerSend(&g_UsbManager, (char*) &data[offset], dataLen - offset);
		if (sent > 0) {
			offset += sent;
		} else {
			return sent;
		}
	}

	return offset;
}

class Connection {
public:
	ConnType mType;

	TcpClientCtx* m_pTcpClientCtx;

	UdpHelperCtx* m_pUdpHelperCtx;

public:
	Connection(ConnType type, void* pCtx) {
		mType = type;
		if (ConnType::TCP == type) {
			m_pTcpClientCtx = (TcpClientCtx*) pCtx;
			m_pUdpHelperCtx = nullptr;
		} else {
			m_pTcpClientCtx = nullptr;
			m_pUdpHelperCtx = (UdpHelperCtx*) pCtx;
		}
	}

	~Connection() {
		if (ConnType::TCP == mType) {
			TcpClientClose(m_pTcpClientCtx);
			TcpClientRelease(m_pTcpClientCtx);

			delete m_pTcpClientCtx;
			m_pTcpClientCtx = nullptr;
		} else {
			UdpHelperClose(m_pUdpHelperCtx);
			UdpHelperRelease(m_pUdpHelperCtx);

			delete m_pUdpHelperCtx;
			m_pUdpHelperCtx = nullptr;
		}
	}
};

map<int, shared_ptr<Connection>> g_NetConnMap;
int32_t g_SendMsgId = 1;

static int32_t getMsgId() {
	int32_t id = g_SendMsgId;
	g_SendMsgId = (g_SendMsgId + 1) % INT32_MAX;

	return id;
}

shared_ptr<Connection> findConnection(int32_t connId) {
	auto it = g_NetConnMap.find(connId);
	if (it == g_NetConnMap.end()) {
		return nullptr;
	}

	return it->second;
}

void removeConnection(int32_t connId) {
	auto it = g_NetConnMap.find(connId);
	if (it != g_NetConnMap.end()) {
		g_NetConnMap.erase(it);
	}
}

void closeAllConnection() {
	g_NetConnMap.clear();
}

void sendMessage(int32_t msgId, int32_t ackId, int32_t connId, ConnType connType,
				 MsgType msgType, const string& ip, int port,
				 int32_t arg1, int32_t arg2, const string& arg3, const string& arg4);

void TcpClientRecvCb(const char* data, int len, void* pCtx) {
	string recvData(data, len);
	auto pTcpClientCtx = (TcpClientCtx*) pCtx;

	sendMessage(0, -1, pTcpClientCtx->conn_id, ConnType::TCP, MsgType::RECV,
				pTcpClientCtx->server_ip, pTcpClientCtx->server_port, 0, 0, "", recvData);
}

void TcpClientErrorCb(int error, const char* des, void* pCtx) {
	string desStr = des;
	auto pTcpClientCtx = (TcpClientCtx*) pCtx;

	sendMessage(0, -1, pTcpClientCtx->conn_id, ConnType::TCP, MsgType::ERROR,
				pTcpClientCtx->server_ip, pTcpClientCtx->server_port, error, 0, desStr, "");
}

void UdpRecvCb(const char* ip, unsigned short port, const char* data, int len, void* pCtx) {
	string ipStr = ip;
	string recvData(data, len);
	auto pUdpClientCtx = (UdpHelperCtx *) pCtx;

	sendMessage(0, -1, pUdpClientCtx->conn_id, ConnType::UDP, MsgType::RECV,
				ip, port, 0, 0, "", recvData);
}

void UdpErrorCb(int error, const char* des, void* pCtx) {
	string desStr = des;
	auto pUdpClientCtx = (UdpHelperCtx*) pCtx;

	sendMessage(0, -1, pUdpClientCtx->conn_id, ConnType::UDP, MsgType::ERROR,
				"", 0, error, 0, desStr, "");
}

string proxyMsgToFrame(std::shared_ptr<ProxyMsg>& msg) {
	string frame = FRAME_BEGIN;
	string frameData = msg->SerializeAsString();
	uint32_t dataLen = htonl(frameData.length());
	frame.append((char*) &dataLen, sizeof(dataLen));
	frame.append(frameData);

	return frame;
}

/**
 * 注意：arg1为错误码，arg2为返回值，arg3为错误描述，arg4为数据。
 */
void processTcpMsg(std::shared_ptr<ProxyMsg>& msg) {
	int32_t connId = msg->connid();
	const string& ip = msg->ip();
	int32_t port = msg->port();

	switch (msg->msgtype()) {
		case MsgType::CREATE: {
			LOG_D("processTcpMsg, CREATE, msgId=%d, ackId=%d, connId=%d, port=%d",
				  msg->msgid(), msg->ackid(), connId, msg->port());

			auto* pTcpClientCtx = new TcpClientCtx;
			pTcpClientCtx->conn_id = connId;
			pTcpClientCtx->local_port = msg->port();
			pTcpClientCtx->recv_cb = TcpClientRecvCb;
			pTcpClientCtx->error_cb = TcpClientErrorCb;
			pTcpClientCtx->recv_buffer = NULL;

			int ret = TcpClientInit(pTcpClientCtx);
			if (SUCCESS != ret) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port, ret, 0, "", "");
				return;
			}

			// save connection to map
			g_NetConnMap[msg->connid()] = make_shared<Connection>(ConnType::TCP, pTcpClientCtx);

			sendMessage(0, msg->msgid(), connId, ConnType::TCP,
						MsgType::RESULT, ip, port,
						0, 0, "create success", "");
		} break;

		case MsgType::CONNECT: {
			LOG_D("processTcpMsg, CONNECT, msgId=%d, ackId=%d, connId=%d, ip=%s, port=%d",
				  msg->msgid(), msg->ackid(), connId, msg->ip().c_str(), msg->port());

			auto pConn = findConnection(connId);
			if (pConn == nullptr) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port,
							ERROR_NO_SUCH_CONN, 0, "no such connection", "");
				return;
			}

			int ret = TcpClientConnect(pConn->m_pTcpClientCtx, ip.c_str(), port);
			if (SUCCESS != ret) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port, ret, 0, "", "");
			}

			sendMessage(0, msg->msgid(), connId, ConnType::TCP,
						MsgType::RESULT, ip, port,
						0, 0, "connect success", "");
		} break;

		case MsgType::SEND: {
			LOG_D("processTcpMsg, SEND, msgId=%d, ackId=%d, connId=%d, port=%d",
				  msg->msgid(), msg->ackid(), connId, msg->port());

			auto pConn = findConnection(connId);
			if (pConn == nullptr) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port,
							ERROR_NO_SUCH_CONN, 0, "no such connection", "");
				return;
			}

			const string& dataToSend = msg->data().arg4();
			int ret = TcpClientSend(pConn->m_pTcpClientCtx, dataToSend.c_str(), dataToSend.length());
			if (ret != dataToSend.length()) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port,
							ret, 0, "send error", "");
			} else {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::RESULT, ip, port,
							0, ret, "send success", "");
			}
		} break;

		case MsgType::CLOSE: {
			LOG_D("processTcpMsg, CLOSE, msgId=%d, ackId=%d, connId=%d, port=%d",
				  msg->msgid(), msg->ackid(), connId, msg->port());

			auto pConn = findConnection(connId);
			if (pConn == nullptr) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port,
							ERROR_NO_SUCH_CONN, 0, "no such connection", "");
				return;
			}

			int ret = TcpClientClose(pConn->m_pTcpClientCtx);
			if (SUCCESS != ret) {
				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::ERROR, ip, port,
							ret, 0, "close error", "");
			} else {
				removeConnection(connId);

				sendMessage(0, msg->msgid(), connId, ConnType::TCP,
							MsgType::RESULT, ip, port,
							0, 0, "close success", "");
			}
		} break;

		case MsgType::RECV:
		case MsgType::ERROR:
		case MsgType::RESULT: {	// send to usb
			msg->set_msgid(getMsgId());

			LOG_D("processTcpMsg, SendToUsb, msgId=%d, ackId=%d, connId=%d, msgType=%d, arg1=%d, arg2=%d, arg3=%s, arg4_len=%d",
				  msg->msgid(), msg->ackid(), connId, msg->msgtype(), msg->data().arg1(), msg->data().arg2(),
				  msg->data().arg3().c_str(), msg->data().arg4().length());

			string frame = proxyMsgToFrame(msg);
			int ret = sendToUsb(frame.c_str(), frame.length());
			LOG_D("sendToUsb, ret=%d", ret);

			if (frame.length() != ret) {
				// TODO usb发送出错
			}
		} break;

		default:
			;
	}
}

void processUdpMsg(std::shared_ptr<ProxyMsg>& msg) {
	int32_t connId = msg->connid();
	const string& ip = msg->ip();
	int32_t port = msg->port();

	switch (msg->msgtype()) {
		case MsgType::CREATE: {
			LOG_D("processUdpMsg, CREATE, msgId=%d, ackId=%d, connId=%d, port=%d",
				  msg->msgid(), msg->ackid(), connId, msg->port());

			auto* pUdpHelperCtx = new UdpHelperCtx;
			pUdpHelperCtx->conn_id = connId;
			pUdpHelperCtx->local_port = port;
			pUdpHelperCtx->recv_buffer = NULL;
			pUdpHelperCtx->recv_cb = UdpRecvCb;
			pUdpHelperCtx->error_cb = UdpErrorCb;

			int ret = UdpHelperInit(pUdpHelperCtx);
			if (SUCCESS != ret) {
				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::ERROR, ip, port, ret, 0, "", "");
				return;
			}

			// save connection to map
			g_NetConnMap[connId] = make_shared<Connection>(ConnType::UDP, pUdpHelperCtx);

			sendMessage(0, msg->msgid(), connId, ConnType::UDP,
						MsgType::RESULT, ip, port,
						0, 0, "create success", "");
		} break;

		case MsgType::SEND: {
			LOG_D("processUdpMsg, SEND, msgId=%d, ackId=%d, connId=%d, ip=%s, port=%d, dataLen=%d",
				  msg->msgid(), msg->ackid(), connId, msg->ip().c_str(), msg->port(), msg->data().arg4().length());

			auto pConn = findConnection(connId);
			if (pConn == nullptr) {
				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::ERROR, ip, port,
							ERROR_NO_SUCH_CONN, 0, "no such connection", "");
				return;
			}

			const string& dataToSend = msg->data().arg4();
			int ret = UdpHelperSend(pConn->m_pUdpHelperCtx, ip.c_str(), port,
									dataToSend.c_str(), dataToSend.length());
			if (ret != dataToSend.length()) {
				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::ERROR, ip, port,
							ret, 0, "send error", "");
			} else {
				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::RESULT, ip, port,
							0, ret, "send success", "");
			}
		} break;

		case MsgType::CLOSE: {
			LOG_D("processUdpMsg, CLOSE, msgId=%d, ackId=%d, connId=%d, port=%d",
				  msg->msgid(), msg->ackid(), connId, msg->port());

			auto pConn = findConnection(connId);
			if (pConn == nullptr) {
				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::ERROR, ip, port,
							ERROR_NO_SUCH_CONN, 0, "no such connection", "");
				return;
			}

			int ret = UdpHelperClose(pConn->m_pUdpHelperCtx);
			if (SUCCESS != ret) {
				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::ERROR, ip, port,
							ret, 0, "close error", "");
			} else {
				removeConnection(connId);

				sendMessage(0, msg->msgid(), connId, ConnType::UDP,
							MsgType::RESULT, ip, port,
							0, 0, "close success", "");
			}
		} break;

		case MsgType::RECV:
		case MsgType::ERROR:
		case MsgType::RESULT: {	// send to usb
			msg->set_msgid(getMsgId());

			LOG_D("processUdpMsg, SendToUsb, msgId=%d, ackId=%d, connId=%d, msgType=%d, arg1=%d, arg2=%d, arg3=%s, arg4_len=%d",
				  msg->msgid(), msg->ackid(), connId, msg->msgtype(), msg->data().arg1(), msg->data().arg2(),
				  msg->data().arg3().c_str(), msg->data().arg4().length());

			string frame = proxyMsgToFrame(msg);
			int ret = sendToUsb(frame.c_str(), frame.length());
			if (frame.length() != ret) {
				// TODO usb发送出错
			}
		} break;

		default:
			;
	}
}

int g_HeartBeatTimoutMs = 3000;

class ProxyMsgHandler : public MessageHandler<ProxyMsg> {
protected:
	void handleMessage(int what, std::shared_ptr<ProxyMsg>& msg) override {
		switch (what) {
			case MSG_PROCESS_PROXY_MESSAGE: {
                if (MsgType::CLOSE_ALL == msg->msgtype()) {
                    // 手机发送的关闭所有
                    LOG_I("close all connection by phone");

                    closeAllConnection();
                } else if (MsgType::USB_HEART_BEAT == msg->msgtype()) {
                    // 去掉之前的超时，再设置新的超时
                    removeMessages(MSG_HEART_BEAT_TIMEOUT);
                    sendEmptyMessage(MSG_HEART_BEAT_TIMEOUT, g_HeartBeatTimoutMs);
                } else {
                    switch (msg->conntype()) {
                        case ConnType::TCP: {
                            processTcpMsg(msg);
                        } break;

                        case ConnType::UDP: {
                            processUdpMsg(msg);
                        } break;

                        default:;
                    }
				}
			} break;

			case MSG_HEART_BEAT_TIMEOUT: {
                LOG_I("heartbeat timeout, close all connection");

                closeAllConnection();
			} break;

			case MSG_CLOSE_ALL_CONN: {
                LOG_I("close all connection");

                closeAllConnection();
			} break;

			default:
				;
		}
	}
};

void sendMessage(int32_t msgId, int32_t ackId, int32_t connId, ConnType connType,
				 MsgType msgType, const string& ip, int port,
				 int32_t arg1, int32_t arg2, const string& arg3, const string& arg4) {
	auto proxyMsg = make_shared<ProxyMsg>();
	proxyMsg->set_msgid(msgId);
	proxyMsg->set_ackid(ackId);
	proxyMsg->set_connid(connId);
	proxyMsg->set_conntype(connType);
	proxyMsg->set_msgtype(msgType);
	proxyMsg->set_ip(ip);
	proxyMsg->set_port(port);

	auto data = proxyMsg->mutable_data();
	data->set_arg1(arg1);
	data->set_arg2(arg2);
	data->set_arg3(arg3);
	data->set_arg4(arg4);

	if (g_pProxyMsgHandler != nullptr) {
		g_pProxyMsgHandler->sendMessage(MSG_PROCESS_PROXY_MESSAGE, proxyMsg);
	}
}

void sendEmptyMessage(int msgType) {
	if (g_pProxyMsgHandler != nullptr) {
		shared_ptr<ProxyMsg> emptyMsg = nullptr;
		g_pProxyMsgHandler->sendMessage(msgType, emptyMsg);
	}
}

void processUsbFrame(const string& frameData) {
	shared_ptr<ProxyMsg> proxyMsg = make_shared<ProxyMsg>();
	if (proxyMsg->ParseFromString(frameData)) {
		LOG_D("processUsbFrame, parse ProxyMsg success");

		if (g_pProxyMsgHandler != nullptr) {
			g_pProxyMsgHandler->sendMessage(MSG_PROCESS_PROXY_MESSAGE, proxyMsg);
		}
	} else {
		LOG_D("processUsbFrame, parse ProxyMsg failed");
	}
}

int main(int argc, char* argv[]) {
	g_UsbManager.recv_cb = UsbManagerRecvCb;
	g_UsbManager.error_cb = UsbManagerErrorCb;

	int ret = UsbManagerInit(&g_UsbManager);
	if (SUCCESS != ret) {
		LOG_E("init usb error");
		return 1;
	}

	// init message handler
	g_pProxyMsgHandler = make_shared<ProxyMsgHandler>();
	g_pProxyMsgHandler->start();

	LOG_I("proxy message handler started");

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

	g_pProxyMsgHandler->stop();
	UsbManagerDestroy(&g_UsbManager);

	sleepMs(3000);

	return 0;
}

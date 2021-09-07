/**
 * Created by huangjian on 2021/9/2.
 */

#ifndef DATAPROXY_ERROR_H
#define DATAPROXY_ERROR_H

const static int SUCCESS			                = 0;
const static int ERROR_FAILED		                = -1;

const static int ERROR_NULL_PTR	                	= 10001;
const static int ERROR_INVALID_FD                 	= 10002;
const static int ERROR_INVALID_OPERATION			= 10003;
const static int ERROR_SELECT						= 10004;
const static int ERROR_SOCK_REMOTE_CLOSE        	= 11001;
const static int ERROR_SOCK_RECV                  	= 11002;
const static int ERROR_SOCK_SEND                  	= 11003;
const static int ERROR_SOCK_ACCEPT                	= 11004;
const static int ERROR_SOCK_TOO_MANY_CLIENTS		= 11005;
const static int ERROR_SOCK_LISTEN					= 11006;
const static int ERROR_USB_RECV						= 12001;
const static int ERROR_USB_SEND						= 12002;
const static int ERROR_QUEUE_FULL                   = 13001;
const static int ERROR_QUEUE_EMPTY                  = 13002;
const static int ERROR_NO_SUCH_CONN					= 14001;

#endif//DATAPROXY_ERROR_H

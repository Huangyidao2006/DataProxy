/**
 * Created by huangjian on 2021/9/2.
 */

#ifndef DATAPROXY_ERROR_H
#define DATAPROXY_ERROR_H

const static int SUCCESS			                = 0;
const static int ERROR_FAILED		                = -1;

const static int ERROR_NULL_PTR	                = 10001;
const static int ERROR_INVALID_FD                 = 10002;
const static int ERROR_SOCK_REMOTE_CLOSE        = 11001;
const static int ERROR_SOCK_RECV                  = 11002;
const static int ERROR_SOCK_SEND                  = 11003;

#endif//DATAPROXY_ERROR_H

/**
 * Created by huangjian on 2021/9/2.
 */

#ifndef DATAPROXY_TIME_H
#define DATAPROXY_TIME_H

long long currentTimeMillis();

/**
 * 获取当前时间。
 *
 * @param fmt 格式，如："%Y-%m-%d %H:%M:%S"
 * @param buffer
 * @param len
 * @return
 */
int getCurrentTime(const char* fmt, char* buffer, int len);

#endif//DATAPROXY_TIME_H

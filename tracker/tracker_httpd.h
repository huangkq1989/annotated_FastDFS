/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 启动Http请求下载服务,使用库libevent的http处理函数
 * 接收下载请求后检查请求是否合法，然后查询文件在哪
 * 一storage,然后发送重定向包
 *
 **************************************************/

/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_httpd.h

#ifndef _TRACKER_HTTPD_H_
#define _TRACKER_HTTPD_H_

#ifdef __cplusplus
extern "C" {
#endif

int tracker_httpd_start(const char *bind_addr);

#ifdef __cplusplus
}
#endif

#endif

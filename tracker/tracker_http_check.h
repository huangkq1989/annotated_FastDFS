/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 开启一条线程连接各storage，检查其http服务是否正常。
 * 不同于心跳包检查storage服务器是否正常运行，需要对
 * http服务进行独立的检查，因为是新线程启动http服务的，
 * TCP模式下通过连接http服务来检查
 * HTTP模式下通过发送HTTP请求来检查
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

#ifndef _TRACKER_HTTP_CHECK_H_
#define _TRACKER_HTTP_CHECK_H_

#ifdef __cplusplus
extern "C" {
#endif

    int tracker_http_check_start();
    int tracker_http_check_stop();

#ifdef __cplusplus
}
#endif

#endif

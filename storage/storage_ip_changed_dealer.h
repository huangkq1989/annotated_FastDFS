/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 当storage的ip改变时需要的处理函数
 * storage启动时，会检查当前连接和之前连接所使用的
 * ip是否一致，不一致的情况下报告Tracker自己原来用
 * ip是什么，Tracker将清空原来的记录这台storage的
 * 数据,以前用这台storage作为新join时用的源头也不
 * 能再用，并把改变记录到changelog
 *
 * 另外，非首次启动的storage需要检查其他机器的ip是
 * 否变了，方法是把changelog拿回来，然后根据changelog
 * 来修改ip_port.mark
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//storage_ip_changed_dealer.h

#ifndef _STORAGE_IP_CHANGED_DEALER_H_
#define _STORAGE_IP_CHANGED_DEALER_H_

#include "tracker_types.h"
#include "tracker_client_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

    int storage_changelog_req();
    int storage_check_ip_changed();

#ifdef __cplusplus
}
#endif

#endif


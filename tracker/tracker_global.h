/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 定义全局变量，这些全局变量大部分由配置文件指定值
 * 另外定义了一些统计数据（*chg_count）
 * 在这里进行声明
 *
 **************************************************/

/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_global.h

#ifndef _TRACKER_GLOBAL_H
#define _TRACKER_GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdfs_define.h"
#include "tracker_types.h"
#include "base64.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

#define TRACKER_SYNC_TO_FILE_FREQ 1000

#ifdef __cplusplus
extern "C" {
#endif

/*是否继续运行*/
extern bool g_continue_flag;
/*在那一端口监听*/
extern int g_server_port;
/*所有组*/
extern FDFSGroups g_groups;
/*storage 状态变更的次数*/
extern int g_storage_stat_chg_count;
/*同步时间戳变更的次数*/
extern int g_storage_sync_time_chg_count; //sync timestamp
/*最大连接数*/
extern int g_max_connections;
/*每个storage需要保留的空闲空间*/
extern int g_storage_reserved_mb;
extern int g_sync_log_buff_interval; //sync log buff to disk every interval seconds
extern int g_check_active_interval; //check storage server alive every interval seconds

extern int g_allow_ip_count;  /* -1 means match any ip address */
extern in_addr_t *g_allow_ip_addrs;  /* sorted array, asc order */
/*用于base64编码*/
extern struct base64_context g_base64_context;

extern char g_run_by_group[32];
extern char g_run_by_user[32];

extern bool g_storage_ip_changed_auto_adjust;
extern bool g_thread_kill_done;

extern int g_thread_stack_size;

#ifdef WITH_HTTPD
extern FDFSHTTPParams g_http_params;
extern int g_http_check_interval;
extern int g_http_check_type;
extern char g_http_check_uri[128];
extern bool g_http_servers_dirty;
#endif

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
extern char g_exe_name[256];
#endif

#ifdef __cplusplus
}
#endif

#endif

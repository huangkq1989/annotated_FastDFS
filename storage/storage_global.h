/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 * 
 * 主要是storage的全局变量
 * 本文件的函数基本与ip有关，因为
 * 文件名有ip地址的字段，需要本机的ip地址，所以启动
 * 的时候会把所有的ip存到全局变量中，供以后使用
 *
 **************************************************/

/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_global.h

#ifndef _STORAGE_GLOBAL_H
#define _STORAGE_GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fdfs_define.h"
#include "tracker_types.h"
#include "client_global.h"
#include "fdht_types.h"
#include "base64.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

/*默认参数*/
#define STORAGE_BEAT_DEF_INTERVAL    30
#define STORAGE_REPORT_DEF_INTERVAL  300
#define STORAGE_DEF_SYNC_WAIT_MSEC   100
#define DEFAULT_SYNC_STAT_FILE_INTERVAL  300
#define DEFAULT_DATA_DIR_COUNT_PER_PATH	 256
#define DEFAULT_UPLOAD_PRIORITY           10
#define DEFAULT_SYNC_MARK_FILE_FREQ  500

#define STORAGE_MAX_LOCAL_IP_ADDRS	  4
#define STORAGE_IF_ALIAS_PREFIX_MAX_SIZE 32

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	FDFSStorageBrief server;
	int last_sync_src_timestamp;
} FDFSStorageServer;

extern bool g_continue_flag;
extern char **g_store_paths; //file store paths
extern int g_path_count;   //store path count

/* subdirs under store path, g_subdir_count * g_subdir_count 2 level subdirs */
extern int g_subdir_count_per_path;

extern int g_server_port;
extern int g_http_port;  //http server port
extern int g_last_server_port;
extern int g_last_http_port;  //last http server port
extern char g_http_domain[FDFS_DOMAIN_NAME_MAX_SIZE];  //http server domain name
extern int g_max_connections;
extern int g_file_distribute_path_mode;
extern int g_file_distribute_rotate_count;
extern int g_fsync_after_written_bytes;

extern int g_dist_path_index_high; //current write to high path
extern int g_dist_path_index_low;  //current write to low path
extern int g_dist_write_file_count; //current write file count

extern int g_storage_count;  //stoage server count in my group
extern FDFSStorageServer g_storage_servers[FDFS_MAX_SERVERS_EACH_GROUP];
extern FDFSStorageServer *g_sorted_storages[FDFS_MAX_SERVERS_EACH_GROUP];

extern char g_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
extern int g_tracker_reporter_count;
extern int g_heart_beat_interval;
extern int g_stat_report_interval;
extern int g_sync_wait_usec;
extern int g_sync_interval; //unit: milliseconds
extern TimeInfo g_sync_start_time;
extern TimeInfo g_sync_end_time;
extern bool g_sync_part_time; //true for part time, false for all time of a day
extern int g_sync_log_buff_interval; //sync log buff to disk every interval seconds
extern int g_sync_binlog_buff_interval; //sync binlog buff to disk every interval seconds
extern int g_write_mark_file_freq;      //write to mark file after sync N files
extern int g_sync_stat_file_interval;   //sync storage stat info to disk interval

extern FDFSStorageStat g_storage_stat;
extern int g_stat_change_count;
extern int g_sync_change_count; //sync src timestamp change counter

extern int g_storage_join_time;
extern bool g_sync_old_done;
extern char g_sync_src_ip_addr[IP_ADDRESS_SIZE];
extern int g_sync_until_timestamp;

extern int g_local_host_ip_count;
/*本storage的所有ip地址*/
extern char g_local_host_ip_addrs[STORAGE_MAX_LOCAL_IP_ADDRS * \
				IP_ADDRESS_SIZE];

extern char g_tracker_client_ip[IP_ADDRESS_SIZE]; //storage ip as tracker client
extern char g_last_storage_ip[IP_ADDRESS_SIZE];	//the last storage ip address

extern int g_allow_ip_count;  /* -1 means match any ip address */
extern in_addr_t *g_allow_ip_addrs;  /* sorted array, asc order */

/*是否去重*/
extern bool g_check_file_duplicate;
/*
 *去重的处理方法是计算出文件hash值作为key，然后存储到FDHT中，
 *为了多个应用能同时存储到FDHT中，key由key_namespace区分
 */
extern char g_key_namespace[FDHT_MAX_NAMESPACE_LEN+1];
extern int g_namespace_len;
extern struct base64_context g_base64_context;

extern char g_run_by_group[32];
extern char g_run_by_user[32];

extern char g_bind_addr[IP_ADDRESS_SIZE];
extern bool g_client_bind_addr;
/*ip变化时是否自动调整*/
extern bool g_storage_ip_changed_auto_adjust;
extern bool g_thread_kill_done;

extern int g_thread_stack_size;
extern int g_upload_priority;
extern time_t g_up_time;
extern char g_if_alias_prefix[STORAGE_IF_ALIAS_PREFIX_MAX_SIZE];

#ifdef WITH_HTTPD
extern FDFSHTTPParams g_http_params;
/*采用http模式提供下载文件时，文件是分块的，每块叫做一个trunk*/
extern int g_http_trunk_size;
#endif

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
extern char g_exe_name[256];
#endif

int storage_cmp_by_ip_addr(const void *p1, const void *p2);

void load_local_host_ip_addrs();
bool is_local_host_ip(const char *client_ip);
int insert_into_local_host_ip(const char *client_ip);
void print_local_host_ip_addrs();

#ifdef __cplusplus
}
#endif

#endif

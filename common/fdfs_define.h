/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 定义fdfs的一些默认参数,如默认端口，进程名
 *
 **************************************************/


/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//fdfs_define.h

#ifndef _FDFS_DEFINE_H_
#define _FDFS_DEFINE_H_

#include <pthread.h>
#include "common_define.h"

#define FDFS_TRACKER_SERVER_DEF_PORT		22000
#define FDFS_STORAGE_SERVER_DEF_PORT		23000
#define FDFS_DEF_STORAGE_RESERVED_MB		1024
#define TRACKER_ERROR_LOG_FILENAME      "trackerd"
#define STORAGE_ERROR_LOG_FILENAME      "storaged"

/*
 *一条记录record有多个字段field，字段间用'\0x01',记录间用'\0x02',
 *这两个字符是不可显示的，不会出现在表示信息的数据中，所以可以用
 */
#define FDFS_RECORD_SEPERATOR	'\x01'
#define FDFS_FIELD_SEPERATOR	'\x02'

#define SYNC_BINLOG_BUFF_DEF_INTERVAL  60
#define CHECK_ACTIVE_DEF_INTERVAL     100

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif


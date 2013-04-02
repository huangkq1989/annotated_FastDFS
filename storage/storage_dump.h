/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 该文件处理的事情很简单，只在调试的情况下才会
 * 起作用，在接收到SIGUSR1和SIGUSR2信号时，把
 * storage的状态信息持久化到文件中，包括以下信息：
 * 1.首先是打印storage全局变量的信息
 * 2.然后打印storag统计数据信息
 * 3.最后打印storage的状态和同步时间戳信息
 *
 **************************************************/

/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_dump.h

#ifndef _STORAGE_DUMP_H
#define _STORAGE_DUMP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdfs_define.h"
#include "tracker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int fdfs_dump_storage_global_vars_to_file(const char *filename);

#ifdef __cplusplus
}
#endif

#endif

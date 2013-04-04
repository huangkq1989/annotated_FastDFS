/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 初始化线程属性和锁
 * 创建和关闭多条工作线程
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

#ifndef PTHREAD_FUNC_H
#define PTHREAD_FUNC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "common_define.h"

#ifdef __cplusplus
extern "C" {
#endif

    int init_pthread_lock(pthread_mutex_t *pthread_lock);
    int init_pthread_attr(pthread_attr_t *pattr, const int stack_size);

    /*创建多条线程,线程id存在tids数组中，kill跟据tids来结束线程*/
    int create_work_threads(int *count, void *(*start_func)(void *), \
            void *arg, pthread_t *tids, const int stack_size);
    int kill_work_threads(pthread_t *tids, const int count);

#ifdef __cplusplus
}
#endif

#endif


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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include "pthread_func.h"
#include "logger.h"

/*初始化锁，并设置锁的属性为PTHREAD_MUTEX_ERRORCHECK,
 *则已经上锁的情况下，同一线程再次上锁，则报EDEADLK错误.
 */
int init_pthread_lock(pthread_mutex_t *pthread_lock)
{
    pthread_mutexattr_t mat;
    int result;

    if ((result=pthread_mutexattr_init(&mat)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutexattr_init fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }
    if ((result=pthread_mutexattr_settype(&mat, \
                    PTHREAD_MUTEX_ERRORCHECK)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutexattr_settype fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }
    if ((result=pthread_mutex_init(pthread_lock, &mat)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_init fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }
    if ((result=pthread_mutexattr_destroy(&mat)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call thread_mutexattr_destroy fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    return 0;
}

/*设置线程的属性，一是设置栈，二是设置线程可以脱离detachable.*/
int init_pthread_attr(pthread_attr_t *pattr, const int stack_size)
{
    size_t old_stack_size;
    size_t new_stack_size;
    int result;

    if ((result=pthread_attr_init(pattr)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_attr_init fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    if ((result=pthread_attr_getstacksize(pattr, &old_stack_size)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_attr_getstacksize fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    if (stack_size > 0)
    {
        if (old_stack_size != stack_size)
        {
            new_stack_size = stack_size;
        }
        else
        {
            new_stack_size = 0;
        }
    }
    else if (old_stack_size < 1 * 1024 * 1024)
    {
        new_stack_size = 1 * 1024 * 1024;
    }
    else
    {
        new_stack_size = 0;
    }

    if (new_stack_size > 0)
    {
        if ((result=pthread_attr_setstacksize(pattr, \
                        new_stack_size)) != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "call pthread_attr_setstacksize fail, " \
                    "errno: %d, error info: %s", \
                    __LINE__, result, strerror(result));
            return result;
        }
    }

    if ((result=pthread_attr_setdetachstate(pattr, \
                    PTHREAD_CREATE_DETACHED)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_attr_setdetachstate fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    return 0;
}

/*创建工作线程, 每条工作线程做的事是一样的，都是调start_func,
 *线程由tids数组标示，以便以后关闭
 */
int create_work_threads(int *count, void *(*start_func)(void *), \
        void *arg, pthread_t *tids, const int stack_size)
{
    int result;
    pthread_attr_t thread_attr;
    pthread_t *ptid;
    pthread_t *ptid_end;

    if ((result=init_pthread_attr(&thread_attr, stack_size)) != 0)
    {
        return result;
    }

    result = 0;
    ptid_end = tids + (*count);
    for (ptid=tids; ptid<ptid_end; ptid++)
    {
        if ((result=pthread_create(ptid, &thread_attr, \
                        start_func, arg)) != 0)
        {
            *count = ptid - tids;
            logError("file: "__FILE__", line: %d, " \
                    "create thread failed, startup threads: %d, " \
                    "errno: %d, error info: %s", \
                    __LINE__, *count, \
                    result, strerror(result));
            break;
        }
    }

    pthread_attr_destroy(&thread_attr);
    return result;
}

/*杀死线程，所要杀死的线程id由tids标示*/
int kill_work_threads(pthread_t *tids, const int count)
{
    int result;
    pthread_t *ptid;
    pthread_t *ptid_end;

    ptid_end = tids + count;
    for (ptid=tids; ptid<ptid_end; ptid++)
    {
        if ((result=pthread_kill(*ptid, SIGINT)) != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "kill thread failed, " \
                    "errno: %d, error info: %s", \
                    __LINE__, result, strerror(result));
        }
    }

    return 0;
}


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

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "logger.h"
#include "fdfs_global.h"
#include "tracker_global.h"
#include "tracker_mem.h"
#include "tracker_proto.h"
#include "http_func.h"
#include "sockopt.h"
#include "tracker_http_check.h"

static pthread_t http_check_tid;

/*发心跳包来检查storage的http服务是否正常*/
static void *http_check_entrance(void *arg)
{
    FDFSGroupInfo *pGroup;
    FDFSGroupInfo *pGroupEnd;
    FDFSStorageDetail **ppServer;
    FDFSStorageDetail **ppServerEnd;
    FDFSStorageDetail *pServer;
    FDFSStorageDetail *pServerEnd;
    char url[512];
    char error_info[512];
    char *content;
    int content_len;
    int http_status;
    int sock;
    int server_count;
    int result;

    g_http_servers_dirty = false;
    while (g_continue_flag)
    {
        if (g_http_servers_dirty)
        {
            g_http_servers_dirty = false;
        }
        else
        {
            sleep(g_http_check_interval);
        }

        pGroupEnd = g_groups.groups + g_groups.count;
        for (pGroup=g_groups.groups; g_continue_flag && (!g_http_servers_dirty)\
                && pGroup<pGroupEnd; pGroup++)
        {

            if (pGroup->storage_http_port <= 0)
            {
                continue;
            }

            server_count = 0;
            ppServerEnd = pGroup->active_servers + pGroup->active_count;
            for (ppServer=pGroup->active_servers; g_continue_flag && \
                    (!g_http_servers_dirty) && ppServer<ppServerEnd; ppServer++)
            {
                /*检查类型为tcp协议*/
                if (g_http_check_type == FDFS_HTTP_CHECK_ALIVE_TYPE_TCP)
                {
                    sock = socket(AF_INET, SOCK_STREAM, 0);
                    if(sock < 0)
                    {
                        result = errno != 0 ? errno : EPERM;
                        logError("file: "__FILE__", line: %d, " \
                                "socket create failed, errno: %d, " \
                                "error info: %s.", \
                                __LINE__, result, strerror(result));
                        sleep(1);
                        continue;
                    }

                    result = connectserverbyip_nb(sock, (*ppServer)->ip_addr, \
                            pGroup->storage_http_port, \
                            g_fdfs_connect_timeout);
                    close(sock);

                    if (g_http_servers_dirty)
                    {
                        break;
                    }

                    /*storage的http服务正常，把该服务器记录pGroup的http_servers中*/
                    if (result == 0)
                    {
                        *(pGroup->http_servers+server_count)=*ppServer;
                        server_count++;
                        if ((*ppServer)->http_check_fail_count > 0)
                        {
                            logInfo("file: "__FILE__", line: %d, " \
                                    "http check alive success " \
                                    "after %d times, server: %s:%d",
                                    __LINE__, \
                                    (*ppServer)->http_check_fail_count, 
                                    (*ppServer)->ip_addr, \
                                    pGroup->storage_http_port);
                            (*ppServer)->http_check_fail_count = 0;
                        }
                    }
                    else
                    {
                        if (result != (*ppServer)->http_check_last_errno)
                        {
                            if ((*ppServer)->http_check_fail_count > 1)
                            {
                                logError("file: "__FILE__", line: %d, "\
                                        "http check alive fail after " \
                                        "%d times, storage server: " \
                                        "%s:%d, error info: %s", \
                                        __LINE__, \
                                        (*ppServer)->http_check_fail_count, \
                                        (*ppServer)->ip_addr, \
                                        pGroup->storage_http_port, \
                                        (*ppServer)->http_check_error_info);
                            }

                            sprintf((*ppServer)->http_check_error_info, 
                                    "http check alive, connect to http " \
                                    "server %s:%d fail, " \
                                    "errno: %d, error info: %s", \
                                    (*ppServer)->ip_addr, \
                                    pGroup->storage_http_port, result, \
                                    strerror(result));

                            logError("file: "__FILE__", line: %d, %s", \
                                    __LINE__, \
                                    (*ppServer)->http_check_error_info);
                            (*ppServer)->http_check_last_errno = result;
                            (*ppServer)->http_check_fail_count = 1;
                        }
                        else
                        {
                            (*ppServer)->http_check_fail_count++;
                        }
                    }
                }
                else  //http
                {
                    sprintf(url, "http://%s:%d%s", (*ppServer)->ip_addr, \
                            pGroup->storage_http_port, g_http_check_uri);

                    /*发送http请求*/
                    result = get_url_content(url, g_fdfs_connect_timeout, \
                            g_fdfs_network_timeout, &http_status, \
                            &content, &content_len, error_info);

                    if (g_http_servers_dirty)
                    {
                        if (result == 0)
                        {
                            free(content);
                        }

                        break;
                    }

                    if (result == 0)
                    {
                        if (http_status == 200)
                        {
                            *(pGroup->http_servers+server_count)=*ppServer;
                            server_count++;

                            if ((*ppServer)->http_check_fail_count > 0)
                            {
                                logInfo("file: "__FILE__", line: %d, " \
                                        "http check alive success " \
                                        "after %d times, url: %s",\
                                        __LINE__, \
                                        (*ppServer)->http_check_fail_count, 
                                        url);
                                (*ppServer)->http_check_fail_count = 0;
                            }
                        }
                        /*http状态码不为200(正常)*/
                        else
                        {
                            /*新的错误状态，重新计数，打印之*/
                            if (http_status != (*ppServer)->http_check_last_status)
                            {
                                if ((*ppServer)->http_check_fail_count > 1)
                                {
                                    logError("file: "__FILE__", line: %d, "\
                                            "http check alive fail after " \
                                            "%d times, storage server: " \
                                            "%s:%d, error info: %s", \
                                            __LINE__, \
                                            (*ppServer)->http_check_fail_count, \
                                            (*ppServer)->ip_addr, \
                                            pGroup->storage_http_port, \
                                            (*ppServer)->http_check_error_info);
                                }

                                sprintf((*ppServer)->http_check_error_info, \
                                        "http check alive fail, url: %s, " \
                                        "http_status=%d", url, http_status);

                                logError("file: "__FILE__", line: %d, %s", \
                                        __LINE__, \
                                        (*ppServer)->http_check_error_info);
                                (*ppServer)->http_check_last_status = http_status;
                                (*ppServer)->http_check_fail_count = 1;
                            }
                            else
                            {
                                (*ppServer)->http_check_fail_count++;
                            }
                        }

                        free(content);
                    }
                    /*result不为0*/
                    else
                    {
                        if (result != (*ppServer)->http_check_last_errno)
                        {
                            if ((*ppServer)->http_check_fail_count > 1)
                            {
                                logError("file: "__FILE__", line: %d, "\
                                        "http check alive fail after " \
                                        "%d times, storage server: " \
                                        "%s:%d, error info: %s", \
                                        __LINE__, \
                                        (*ppServer)->http_check_fail_count, \
                                        (*ppServer)->ip_addr, \
                                        pGroup->storage_http_port, \
                                        (*ppServer)->http_check_error_info);
                            }

                            sprintf((*ppServer)->http_check_error_info, \
                                    "http check alive fail, " \
                                    "error info: %s", error_info);

                            logError("file: "__FILE__", line: %d, %s", \
                                    __LINE__, \
                                    (*ppServer)->http_check_error_info);
                            (*ppServer)->http_check_last_errno = result;
                            (*ppServer)->http_check_fail_count = 1;
                        }
                        else
                        {
                            (*ppServer)->http_check_fail_count++;
                        }
                    }
                }
            }

            if (g_http_servers_dirty)
            {
                break;
            }

            if (pGroup->http_server_count != server_count)
            {
                logDebug("file: "__FILE__", line: %d, " \
                        "group: %s, HTTP server count change from %d to %d", \
                        __LINE__, pGroup->group_name, \
                        pGroup->http_server_count, server_count);

                pGroup->http_server_count = server_count;
            }
        }
    }

    /*开始停止tracker,打印所有storage的出错信息*/
    pGroupEnd = g_groups.groups + g_groups.count;
    for (pGroup=g_groups.groups; pGroup<pGroupEnd; pGroup++)
    {
        pServerEnd = pGroup->all_servers + pGroup->count;
        for (pServer=pGroup->all_servers; pServer<pServerEnd; pServer++)
        {
            if (pServer->http_check_fail_count > 1)
            {
                logError("file: "__FILE__", line: %d, " \
                        "http check alive fail " \
                        "after %d times, storage server: %s:%d, " \
                        "error info: %s", \
                        __LINE__, pServer->http_check_fail_count, \
                        pServer->ip_addr, pGroup->storage_http_port, \
                        pServer->http_check_error_info);
            }
        }
    }

    return NULL;
}

int tracker_http_check_start()
{
    int result;

    /*不检查*/
    if (g_http_check_interval <= 0)
    {
        return 0;
    }

    if ((result=pthread_create(&http_check_tid, NULL, \
                    http_check_entrance, NULL)) != 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "create thread failed, errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    return 0;
}

int tracker_http_check_stop()
{
    /*不检查*/
    if (g_http_check_interval <= 0)
    {
        return 0;
    }

    return pthread_kill(http_check_tid, SIGINT);
}


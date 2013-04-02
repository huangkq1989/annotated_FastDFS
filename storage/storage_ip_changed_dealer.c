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


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "shared_func.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_ip_changed_dealer.h"

static int storage_do_changelog_req(TrackerServerInfo *pTrackerServer)
{
    char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN];
    TrackerHeader *pHeader;
    int result;

    memset(out_buff, 0, sizeof(out_buff));
    pHeader = (TrackerHeader *)out_buff;

    long2buff(FDFS_GROUP_NAME_MAX_LEN, pHeader->pkg_len);
    pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_CHANGELOG_REQ;
    /*发组名过去*/
    strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
    if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
                    sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN, \
                    g_fdfs_network_timeout)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "tracker server %s:%d, send data fail, " \
                "errno: %d, error info: %s.", \
                __LINE__, pTrackerServer->ip_addr, \
                pTrackerServer->port, \
                result, strerror(result));
        return result;
    }

    return tracker_deal_changelog_response(pTrackerServer);
}

static int storage_report_ip_changed(TrackerServerInfo *pTrackerServer)
{
    char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
        2 * IP_ADDRESS_SIZE];
    char in_buff[1];
    char *pInBuff;
    TrackerHeader *pHeader;
    int result;
    int64_t in_bytes;

    memset(out_buff, 0, sizeof(out_buff));
    pHeader = (TrackerHeader *)out_buff;

    long2buff(FDFS_GROUP_NAME_MAX_LEN+2*IP_ADDRESS_SIZE, pHeader->pkg_len);
    pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_REPORT_IP_CHANGED;
    strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
    strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN, \
            g_last_storage_ip);
    strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
            IP_ADDRESS_SIZE, g_tracker_client_ip);

    /*报告ip变了*/
    if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
                    sizeof(out_buff), g_fdfs_network_timeout)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "tracker server %s:%d, send data fail, " \
                "errno: %d, error info: %s", \
                __LINE__, pTrackerServer->ip_addr, \
                pTrackerServer->port, result, strerror(result));
        return result;
    }

    pInBuff = in_buff;
    result = fdfs_recv_response(pTrackerServer, \
            &pInBuff, 0, &in_bytes);

    if (result == 0 || result == EALREADY || result == ENOENT)
    {
        return 0;
    }
    else
    {
        logError("file: "__FILE__", line: %d, " \
                "tracker server %s:%d, recv data fail or " \
                "response status != 0, " \
                "errno: %d, error info: %s", \
                __LINE__, pTrackerServer->ip_addr, \
                pTrackerServer->port, result, strerror(result));
        return result == EBUSY ? 0 : result;
    }
}

static int storage_report_storage_ip_addr()
{
    TrackerServerInfo *pGlobalServer;
    TrackerServerInfo *pTServer;
    TrackerServerInfo *pTServerEnd;
    TrackerServerInfo trackerServer;
    char tracker_client_ip[IP_ADDRESS_SIZE];
    int success_count;
    int result;
    int i;

    success_count = 0;
    pTServer = &trackerServer;
    pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;

    /*成功一次就成了,好些地方都有以下的代码，其实应该
     *封装以下的
     */
    while (success_count == 0 && g_continue_flag)
    {
        /*处理网络连接*/
        for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
                pGlobalServer++)
        {
            memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
            for (i=0; i < 3; i++)
            {
                pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
                if(pTServer->sock < 0)
                {
                    result = errno != 0 ? errno : EPERM;
                    logError("file: "__FILE__", line: %d, " \
                            "socket create failed, errno: %d, " \
                            "error info: %s.", \
                            __LINE__, result, strerror(result));
                    sleep(5);
                    break;
                }

                if (g_client_bind_addr && *g_bind_addr != '\0')
                {
                    socketBind(pTServer->sock, g_bind_addr, 0);
                }

                if ((result=connectserverbyip_nb(pTServer->sock, \
                                pTServer->ip_addr, pTServer->port, \
                                g_fdfs_connect_timeout)) == 0)
                {
                    break;
                }

                close(pTServer->sock);
                pTServer->sock = -1;
                sleep(5);
            }

            if (pTServer->sock < 0)
            {
                logError("file: "__FILE__", line: %d, " \
                        "connect to tracker server %s:%d fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, pTServer->ip_addr, pTServer->port, \
                        result, strerror(result));

                continue;
            }

            /*拿到本机用于连接tracker的ip*/
            getSockIpaddr(pTServer->sock, tracker_client_ip, IP_ADDRESS_SIZE);
            /*连接到第一个tracker*/
            if (*g_tracker_client_ip == '\0')
            {
                strcpy(g_tracker_client_ip, tracker_client_ip);
            }
            /*必须保证连接到所有tracker的本机的ip是一样的*/
            else if (strcmp(tracker_client_ip, g_tracker_client_ip) != 0)
            {
                logError("file: "__FILE__", line: %d, " \
                        "as a client of tracker server %s:%d, " \
                        "my ip: %s != client ip: %s of other " \
                        "tracker client", __LINE__, \
                        pTServer->ip_addr, pTServer->port, \
                        tracker_client_ip, g_tracker_client_ip);

                close(pTServer->sock);
                return EINVAL;
            }

            fdfs_quit(pTServer);
            close(pTServer->sock);
            success_count++;
        }
    }

    logDebug("file: "__FILE__", line: %d, " \
            "last my ip is %s, current my ip is %s", \
            __LINE__, g_last_storage_ip, g_tracker_client_ip);

    /*首次启动*/
    if (*g_last_storage_ip == '\0')
    {
        return storage_write_to_sync_ini_file();
    }
    /*如果没变化，返回*/
    else if (strcmp(g_tracker_client_ip, g_last_storage_ip) == 0)
    {
        return 0;
    }

    /*有变化*/
    success_count = 0;
    while (success_count == 0 && g_continue_flag)
    {
        for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
                pGlobalServer++)
        {
            memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
            /*重复代码*/
            for (i=0; i < 3; i++)
            {
                pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
                if(pTServer->sock < 0)
                {
                    result = errno != 0 ? errno : EPERM;
                    logError("file: "__FILE__", line: %d, " \
                            "socket create failed, errno: %d, " \
                            "error info: %s.", \
                            __LINE__, result, strerror(result));
                    sleep(5);
                    break;
                }

                if (g_client_bind_addr && *g_bind_addr != '\0')
                {
                    socketBind(pTServer->sock, g_bind_addr, 0);
                }

                if (tcpsetnonblockopt(pTServer->sock) != 0)
                {
                    close(pTServer->sock);
                    pTServer->sock = -1;
                    sleep(1);
                    continue;
                }

                if ((result=connectserverbyip_nb(pTServer->sock, \
                                pTServer->ip_addr, pTServer->port, \
                                g_fdfs_connect_timeout)) == 0)
                {
                    break;
                }

                close(pTServer->sock);
                pTServer->sock = -1;
                sleep(1);
            }

            if (pTServer->sock < 0)
            {
                logError("file: "__FILE__", line: %d, " \
                        "connect to tracker server %s:%d fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, pTServer->ip_addr, pTServer->port, \
                        result, strerror(result));

                continue;
            }

            /*报告ip变了*/
            if ((result=storage_report_ip_changed(pTServer)) == 0)
            {
                success_count++;
            }
            else
            {
                sleep(1);
            }

            fdfs_quit(pTServer);
            close(pTServer->sock);
        }
    }

    if (!g_continue_flag)
    {
        return EINTR;
    }

    return storage_write_to_sync_ini_file();
}

int storage_changelog_req()
{
    TrackerServerInfo *pGlobalServer;
    TrackerServerInfo *pTServer;
    TrackerServerInfo *pTServerEnd;
    TrackerServerInfo trackerServer;
    int success_count;
    int result;
    int i;

    success_count = 0;
    pTServer = &trackerServer;
    pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;

    while (success_count == 0 && g_continue_flag)
    {
        for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
                pGlobalServer++)
        {
            memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
            for (i=0; i < 3; i++)
            {
                pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
                if(pTServer->sock < 0)
                {
                    result = errno != 0 ? errno : EPERM;
                    logError("file: "__FILE__", line: %d, " \
                            "socket create failed, errno: %d, " \
                            "error info: %s.", \
                            __LINE__, result, strerror(result));
                    sleep(5);
                    break;
                }

                if (g_client_bind_addr && *g_bind_addr != '\0')
                {
                    socketBind(pTServer->sock, g_bind_addr, 0);
                }

                if (tcpsetnonblockopt(pTServer->sock) != 0)
                {
                    close(pTServer->sock);
                    pTServer->sock = -1;
                    sleep(1);
                    continue;
                }

                if ((result=connectserverbyip_nb(pTServer->sock, \
                                pTServer->ip_addr, pTServer->port, \
                                g_fdfs_connect_timeout)) == 0)
                {
                    break;
                }

                close(pTServer->sock);
                pTServer->sock = -1;
                sleep(1);
            }

            if (pTServer->sock < 0)
            {
                logError("file: "__FILE__", line: %d, " \
                        "connect to tracker server %s:%d fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, pTServer->ip_addr, pTServer->port, \
                        result, strerror(result));

                continue;
            }

            /*要changelog*/
            result = storage_do_changelog_req(pTServer);
            if (result == 0 || result == ENOENT)
            {
                success_count++;
            }
            else
            {
                sleep(1);
            }

            fdfs_quit(pTServer);
            close(pTServer->sock);
        }
    }

    if (!g_continue_flag)
    {
        return EINTR;
    }

    return 0;
}

/*检查ip是否变了*/
int storage_check_ip_changed()
{
    int result;

    /*不用处理*/
    if (!g_storage_ip_changed_auto_adjust)
    {
        return 0;
    }

    /*报告ip地址，返回值不为0时，表示出错*/
    if ((result=storage_report_storage_ip_addr()) != 0)
    {
        /*出错了*/
        return result;
    }

    /*第一次，没有mark文件要检查*/
    if (*g_last_storage_ip == '\0') //first run
    {
        return 0;
    }

    /*否则，请求changelog*/
    return storage_changelog_req();
}


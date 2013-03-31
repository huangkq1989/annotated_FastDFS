/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 本文件处理加载配置文件，加载后把参数赋给Tracker的
 * 全局变量
 *
 **************************************************/
/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//tracker_func.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "shared_func.h"
#include "ini_file_reader.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_global.h"
#include "tracker_func.h"
#include "tracker_mem.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

/**
 * 上传文件的模式有三种：
 * 1. Round robin
 * 2. 负载均衡
 * 3. 选择指定组，在这种模式下必须记录指向的是哪个组
 */
static int tracker_load_store_lookup(const char *filename, \
        IniContext *pItemContext)
{
    char *pGroupName;
    g_groups.store_lookup = iniGetIntValue(NULL, "store_lookup", \
            pItemContext, FDFS_STORE_LOOKUP_ROUND_ROBIN);
    if (g_groups.store_lookup == FDFS_STORE_LOOKUP_ROUND_ROBIN)
    {
        /*无需记录*/
        g_groups.store_group[0] = '\0';
        return 0;
    }

    if (g_groups.store_lookup == FDFS_STORE_LOOKUP_LOAD_BALANCE)
    {
        /*无需记录*/
        g_groups.store_group[0] = '\0';
        return 0;
    }

    if (g_groups.store_lookup != FDFS_STORE_LOOKUP_SPEC_GROUP)
    {
        logError("file: "__FILE__", line: %d, " \
                "conf file \"%s\", the value of \"store_lookup\" is " \
                "invalid, value=%d!", \
                __LINE__, filename, g_groups.store_lookup);
        return EINVAL;
    }

    /*从配置文件中获取指定组组名*/
    pGroupName = iniGetStrValue(NULL, "store_group", pItemContext);
    if (pGroupName == NULL)
    {
        logError("file: "__FILE__", line: %d, " \
                "conf file \"%s\" must have item " \
                "\"store_group\"!", __LINE__, filename);
        return ENOENT;
    }
    if (pGroupName[0] == '\0')
    {
        logError("file: "__FILE__", line: %d, " \
                "conf file \"%s\", " \
                "store_group is empty!", __LINE__, filename);
        return EINVAL;
    }

    snprintf(g_groups.store_group, sizeof(g_groups.store_group), \
            "%s", pGroupName);
    /*检验组名是否合法*/
    if (fdfs_validate_group_name(g_groups.store_group) != 0) \
    {
        logError("file: "__FILE__", line: %d, " \
                "conf file \"%s\", " \
                "the group name \"%s\" is invalid!", \
                __LINE__, filename, g_groups.store_group);
        return EINVAL;
    }

    return 0;
}

/*加载配置文件*/
int tracker_load_from_conf_file(const char *filename, \
        char *bind_addr, const int addr_size)
{
    char *pBasePath;
    char *pBindAddr;
    char *pStorageReserved;
    char *pRunByGroup;
    char *pRunByUser;
    char *pThreadStackSize;
#ifdef WITH_HTTPD
    char *pHttpCheckUri;
    char *pHttpCheckType;
#endif
    IniContext iniContext;
    int result;
    int64_t storage_reserved;
    int64_t thread_stack_size;

    memset(&iniContext, 0, sizeof(IniContext));
    if ((result=iniLoadFromFile(filename, &iniContext)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "load conf file \"%s\" fail, ret code: %d", \
                __LINE__, filename, result);
        return result;
    }

    //iniPrintItems(&iniContext);

    do
    {
        if (iniGetBoolValue(NULL, "disabled", &iniContext, false))
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file \"%s\" disabled=true, exit", \
                    __LINE__, filename);
            result = ECANCELED;
            break;
        }

        pBasePath = iniGetStrValue(NULL, "base_path", &iniContext);
        if (pBasePath == NULL)
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file \"%s\" must have item " \
                    "\"base_path\"!", __LINE__, filename);
            result = ENOENT;
            break;
        }

        snprintf(g_fdfs_base_path, sizeof(g_fdfs_base_path), "%s", pBasePath);
        chopPath(g_fdfs_base_path);
        if (!fileExists(g_fdfs_base_path))
        {
            logError("file: "__FILE__", line: %d, " \
                    "\"%s\" can't be accessed, error info: %s", \
                    __LINE__, g_fdfs_base_path, strerror(errno));
            result = errno != 0 ? errno : ENOENT;
            break;
        }
        if (!isDir(g_fdfs_base_path))
        {
            logError("file: "__FILE__", line: %d, " \
                    "\"%s\" is not a directory!", \
                    __LINE__, g_fdfs_base_path);
            result = ENOTDIR;
            break;
        }

        load_log_level(&iniContext);
        if ((result=log_set_prefix(g_fdfs_base_path, \
                        TRACKER_ERROR_LOG_FILENAME)) != 0)
        {
            break;
        }

        g_fdfs_connect_timeout = iniGetIntValue(NULL, "connect_timeout", \
                &iniContext, DEFAULT_CONNECT_TIMEOUT);
        if (g_fdfs_connect_timeout <= 0)
        {
            g_fdfs_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
        }

        g_fdfs_network_timeout = iniGetIntValue(NULL, "network_timeout", \
                &iniContext, DEFAULT_NETWORK_TIMEOUT);
        if (g_fdfs_network_timeout <= 0)
        {
            g_fdfs_network_timeout = DEFAULT_NETWORK_TIMEOUT;
        }

        g_server_port = iniGetIntValue(NULL, "port", &iniContext, \
                FDFS_TRACKER_SERVER_DEF_PORT);
        if (g_server_port <= 0)
        {
            g_server_port = FDFS_TRACKER_SERVER_DEF_PORT;
        }

        pBindAddr = iniGetStrValue(NULL, "bind_addr", &iniContext);
        if (pBindAddr == NULL)
        {
            bind_addr[0] = '\0';
        }
        else
        {
            snprintf(bind_addr, addr_size, "%s", pBindAddr);
        }

        /*处理上传时定位storage group的模式*/
        if ((result=tracker_load_store_lookup(filename, \
                        &iniContext)) != 0)
        {
            break;
        }

        /*处理上传时定位storage 的模式*/
        g_groups.store_server = (byte)iniGetIntValue(NULL, \
                "store_server",  &iniContext, \
                FDFS_STORE_SERVER_ROUND_ROBIN);
        if (!(g_groups.store_server == FDFS_STORE_SERVER_FIRST_BY_IP ||\
                    g_groups.store_server == FDFS_STORE_SERVER_FIRST_BY_PRI||
                    g_groups.store_server == FDFS_STORE_SERVER_ROUND_ROBIN))
        {
            logWarning("file: "__FILE__", line: %d, " \
                    "store_server 's value %d is invalid, " \
                    "set to %d (round robin)!", \
                    __LINE__, g_groups.store_server, \
                    FDFS_STORE_SERVER_ROUND_ROBIN);

            g_groups.store_server = FDFS_STORE_SERVER_ROUND_ROBIN;
        }

        /*处理下载时定位storage 的模式*/
        g_groups.download_server = (byte)iniGetIntValue(NULL, \
                "download_server", &iniContext, \
                FDFS_DOWNLOAD_SERVER_ROUND_ROBIN);
        if (!(g_groups.download_server==FDFS_DOWNLOAD_SERVER_ROUND_ROBIN
                    || g_groups.download_server == 
                    FDFS_DOWNLOAD_SERVER_SOURCE_FIRST))
        {
            logWarning("file: "__FILE__", line: %d, " \
                    "download_server 's value %d is invalid, " \
                    "set to %d (round robin)!", \
                    __LINE__, g_groups.download_server, \
                    FDFS_DOWNLOAD_SERVER_ROUND_ROBIN);

            g_groups.download_server = \
                                       FDFS_DOWNLOAD_SERVER_ROUND_ROBIN;
        }

        /*处理上传时定位path 的模式*/
        g_groups.store_path = (byte)iniGetIntValue(NULL, "store_path", \
                &iniContext, FDFS_STORE_PATH_ROUND_ROBIN);
        if (!(g_groups.store_path == FDFS_STORE_PATH_ROUND_ROBIN || \
                    g_groups.store_path == FDFS_STORE_PATH_LOAD_BALANCE))
        {
            logWarning("file: "__FILE__", line: %d, " \
                    "store_path 's value %d is invalid, " \
                    "set to %d (round robin)!", \
                    __LINE__, g_groups.store_path , \
                    FDFS_STORE_PATH_ROUND_ROBIN);
            g_groups.store_path = FDFS_STORE_PATH_ROUND_ROBIN;
        }

        pStorageReserved = iniGetStrValue(NULL, \
                "reserved_storage_space", &iniContext);
        if (pStorageReserved == NULL)
        {
            g_storage_reserved_mb = FDFS_DEF_STORAGE_RESERVED_MB;
        }
        else
        {
            if ((result=parse_bytes(pStorageReserved, 1, \
                            &storage_reserved)) != 0)
            {
                return result;
            }

            g_storage_reserved_mb = storage_reserved / FDFS_ONE_MB;
        }

        g_max_connections = iniGetIntValue(NULL, "max_connections", \
                &iniContext, DEFAULT_MAX_CONNECTONS);
        if (g_max_connections <= 0)
        {
            g_max_connections = DEFAULT_MAX_CONNECTONS;
        }

        if ((result=set_rlimit(RLIMIT_NOFILE, g_max_connections)) != 0)
        {
            break;
        }

        pRunByGroup = iniGetStrValue(NULL, "run_by_group", &iniContext);
        pRunByUser = iniGetStrValue(NULL, "run_by_user", &iniContext);
        if (pRunByGroup == NULL)
        {
            *g_run_by_group = '\0';
        }
        else
        {
            snprintf(g_run_by_group, sizeof(g_run_by_group), \
                    "%s", pRunByGroup);
        }

        if (pRunByUser == NULL)
        {
            *g_run_by_user = '\0';
        }
        else
        {
            snprintf(g_run_by_user, sizeof(g_run_by_user), \
                    "%s", pRunByUser);
        }

        if ((result=load_allow_hosts(&iniContext, \
                        &g_allow_ip_addrs, &g_allow_ip_count)) != 0)
        {
            return result;
        }

        g_sync_log_buff_interval = iniGetIntValue(NULL, \
                "sync_log_buff_interval", &iniContext, \
                SYNC_LOG_BUFF_DEF_INTERVAL);
        if (g_sync_log_buff_interval <= 0)
        {
            g_sync_log_buff_interval = SYNC_LOG_BUFF_DEF_INTERVAL;
        }

        g_check_active_interval = iniGetIntValue(NULL, \
                "check_active_interval", &iniContext, \
                CHECK_ACTIVE_DEF_INTERVAL);
        if (g_check_active_interval <= 0)
        {
            g_check_active_interval = CHECK_ACTIVE_DEF_INTERVAL;
        }

        pThreadStackSize = iniGetStrValue(NULL, \
                "thread_stack_size", &iniContext);
        if (pThreadStackSize == NULL)
        {
            thread_stack_size = 64 * 1024;
        }
        else if ((result=parse_bytes(pThreadStackSize, 1, \
                        &thread_stack_size)) != 0)
        {
            return result;
        }
        g_thread_stack_size = (int)thread_stack_size;

        g_storage_ip_changed_auto_adjust = iniGetBoolValue(NULL, \
                "storage_ip_changed_auto_adjust", \
                &iniContext, true);

#ifdef WITH_HTTPD
        if ((result=fdfs_http_params_load(&iniContext, \
                        filename, &g_http_params)) != 0)
        {
            return result;
        }

        g_http_check_interval = iniGetIntValue(NULL, \
                "http.check_alive_interval", &iniContext, 30);

        pHttpCheckType = iniGetStrValue(NULL, \
                "http.check_alive_type", &iniContext);
        if (pHttpCheckType != NULL && \
                strcasecmp(pHttpCheckType, "http") == 0)
        {
            g_http_check_type = FDFS_HTTP_CHECK_ALIVE_TYPE_HTTP;
        }
        else
        {
            g_http_check_type = FDFS_HTTP_CHECK_ALIVE_TYPE_TCP;
        }

        pHttpCheckUri = iniGetStrValue(NULL, \
                "http.check_alive_uri", &iniContext);
        if (pHttpCheckUri == NULL)
        {
            *g_http_check_uri = '/';
            *(g_http_check_uri+1) = '\0';
        }
        else if (*pHttpCheckUri == '/')
        {
            snprintf(g_http_check_uri, sizeof(g_http_check_uri), \
                    "%s", pHttpCheckUri);
        }
        else
        {
            snprintf(g_http_check_uri, sizeof(g_http_check_uri), \
                    "/%s", pHttpCheckUri);
        }


#endif

        logInfo("FastDFS v%d.%d, base_path=%s, " \
                "connect_timeout=%ds, "    \
                "network_timeout=%ds, "    \
                "port=%d, bind_addr=%s, " \
                "max_connections=%d, "    \
                "store_lookup=%d, store_group=%s, " \
                "store_server=%d, store_path=%d, " \
                "reserved_storage_space=%dMB, " \
                "download_server=%d, " \
                "allow_ip_count=%d, sync_log_buff_interval=%ds, " \
                "check_active_interval=%ds, " \
                "thread_stack_size=%d KB, " \
                "storage_ip_changed_auto_adjust=%d",  \
                g_fdfs_version.major, g_fdfs_version.minor,  \
                g_fdfs_base_path, g_fdfs_connect_timeout, \
                g_fdfs_network_timeout, \
                g_server_port, bind_addr, g_max_connections, \
                g_groups.store_lookup, g_groups.store_group, \
                g_groups.store_server, g_groups.store_path, \
                g_storage_reserved_mb, g_groups.download_server, \
                g_allow_ip_count, g_sync_log_buff_interval, \
                g_check_active_interval, g_thread_stack_size / 1024, \
                g_storage_ip_changed_auto_adjust);

#ifdef WITH_HTTPD
        if (!g_http_params.disabled)
        {
            logInfo("HTTP supported: " \
                    "server_port=%d, " \
                    "default_content_type=%s, " \
                    "anti_steal_token=%d, " \
                    "token_ttl=%ds, " \
                    "anti_steal_secret_key length=%d, "  \
                    "token_check_fail content_type=%s, " \
                    "token_check_fail buff length=%d, "  \
                    "check_active_interval=%d, " \
                    "check_active_type=%s, " \
                    "check_active_uri=%s",  \
                    g_http_params.server_port, \
                    g_http_params.default_content_type, \
                    g_http_params.anti_steal_token, \
                    g_http_params.token_ttl, \
                    g_http_params.anti_steal_secret_key.length, \
                    g_http_params.token_check_fail_content_type, \
                    g_http_params.token_check_fail_buff.length, \
                    g_http_check_interval, g_http_check_type == \
                    FDFS_HTTP_CHECK_ALIVE_TYPE_TCP ? "tcp":"http",\
                    g_http_check_uri);
        }
#endif

    } while (0);

    iniFreeContext(&iniContext);

    return result;
}


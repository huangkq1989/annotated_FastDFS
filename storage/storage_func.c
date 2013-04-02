/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 * 
 * 一些跟文件相关的处理函数
 * 主要解释的是storage_func_init()函数，该函数处理了：
 * 读取配置，检查ip变化，获取changelog，读取state文件
 * 在 storage刚启动时被调用
 * 
 * 其他函数函数名基本能说明其作用了，不做注释 
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//storage_func.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
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
#include "pthread_func.h"
#include "ini_file_reader.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_param_getter.h"
#include "storage_ip_changed_dealer.h"
#include "fdht_global.h"
#include "fdht_func.h"
#include "fdht_client.h"
#include "client_func.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

#define DATA_DIR_INITED_FILENAME	".data_init_flag"
#define STORAGE_STAT_FILENAME		"storage_stat.dat"

#define INIT_ITEM_STORAGE_JOIN_TIME	"storage_join_time"
#define INIT_ITEM_SYNC_OLD_DONE		"sync_old_done"
#define INIT_ITEM_SYNC_SRC_SERVER	"sync_src_server"
#define INIT_ITEM_SYNC_UNTIL_TIMESTAMP	"sync_until_timestamp"
#define INIT_ITEM_LAST_IP_ADDRESS	"last_ip_addr"
#define INIT_ITEM_LAST_SERVER_PORT	"last_server_port"
#define INIT_ITEM_LAST_HTTP_PORT	"last_http_port"

#define STAT_ITEM_TOTAL_UPLOAD		"total_upload_count"
#define STAT_ITEM_SUCCESS_UPLOAD	"success_upload_count"
#define STAT_ITEM_TOTAL_DOWNLOAD	"total_download_count"
#define STAT_ITEM_SUCCESS_DOWNLOAD	"success_download_count"
#define STAT_ITEM_LAST_SOURCE_UPD	"last_source_update"
#define STAT_ITEM_LAST_SYNC_UPD		"last_sync_update"
#define STAT_ITEM_TOTAL_SET_META	"total_set_meta_count"
#define STAT_ITEM_SUCCESS_SET_META	"success_set_meta_count"
#define STAT_ITEM_TOTAL_DELETE		"total_delete_count"
#define STAT_ITEM_SUCCESS_DELETE	"success_delete_count"
#define STAT_ITEM_TOTAL_GET_META	"total_get_meta_count"
#define STAT_ITEM_SUCCESS_GET_META	"success_get_meta_count"
#define STAT_ITEM_TOTAL_CREATE_LINK	"total_create_link_count"
#define STAT_ITEM_SUCCESS_CREATE_LINK	"success_create_link_count"
#define STAT_ITEM_TOTAL_DELETE_LINK	"total_delete_link_count"
#define STAT_ITEM_SUCCESS_DELETE_LINK	"success_delete_link_count"

#define STAT_ITEM_DIST_PATH_INDEX_HIGH	"dist_path_index_high"
#define STAT_ITEM_DIST_PATH_INDEX_LOW	"dist_path_index_low"
#define STAT_ITEM_DIST_WRITE_FILE_COUNT	"dist_write_file_count"

static int storage_stat_fd = -1;

/*
   static pthread_mutex_t fsync_thread_mutex;
   static pthread_cond_t fsync_thread_cond;
   static int fsync_thread_count = 0;
   */

static pthread_mutex_t sync_stat_file_lock;

static int storage_open_stat_file();
static int storage_close_stat_file();
static int storage_make_data_dirs(const char *pBasePath);
static int storage_check_and_make_data_dirs();


/*storage_stat.dat*/
static char *get_storage_stat_filename(const void *pArg, char *full_filename)
{
    static char buff[MAX_PATH_SIZE];

    if (full_filename == NULL)
    {
        full_filename = buff;
    }

    snprintf(full_filename, MAX_PATH_SIZE, \
            "%s/data/%s", g_fdfs_base_path, STORAGE_STAT_FILENAME);
    return full_filename;
}

/*把缓冲buff写入fd*/
int storage_write_to_fd(int fd, get_filename_func filename_func, \
        const void *pArg, const char *buff, const int len)
{
    if (ftruncate(fd, 0) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "truncate file \"%s\" to empty fail, " \
                "error no: %d, error info: %s", \
                __LINE__, filename_func(pArg, NULL), \
                errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    if (lseek(fd, 0, SEEK_SET) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "rewind file \"%s\" to start fail, " \
                "error no: %d, error info: %s", \
                __LINE__, filename_func(pArg, NULL), \
                errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    if (write(fd, buff, len) != len)
    {
        logError("file: "__FILE__", line: %d, " \
                "write to file \"%s\" fail, " \
                "error no: %d, error info: %s", \
                __LINE__, filename_func(pArg, NULL), \
                errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    if (fsync(fd) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "sync file \"%s\" to disk fail, " \
                "error no: %d, error info: %s", \
                __LINE__, filename_func(pArg, NULL), \
                errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    return 0;
}

/*加载统计信息*/
static int storage_open_stat_file()
{
    char full_filename[MAX_PATH_SIZE];
    IniContext iniContext;
    int result;

    get_storage_stat_filename(NULL, full_filename);
    if (fileExists(full_filename))
    {
        if ((result=iniLoadFromFile(full_filename, &iniContext)) \
                != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "load from stat file \"%s\" fail, " \
                    "error code: %d", \
                    __LINE__, full_filename, result);
            return result;
        }

        if (iniContext.global.count < 12)
        {
            iniFreeContext(&iniContext);
            logError("file: "__FILE__", line: %d, " \
                    "in stat file \"%s\", item count: %d < 12", \
                    __LINE__, full_filename, iniContext.global.count);
            return ENOENT;
        }

        g_storage_stat.total_upload_count = iniGetInt64Value(NULL, \
                STAT_ITEM_TOTAL_UPLOAD, \
                &iniContext, 0);
        g_storage_stat.success_upload_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_UPLOAD, \
                &iniContext, 0);
        g_storage_stat.total_download_count = iniGetInt64Value(NULL,  \
                STAT_ITEM_TOTAL_DOWNLOAD, \
                &iniContext, 0);
        g_storage_stat.success_download_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_DOWNLOAD, \
                &iniContext, 0);
        g_storage_stat.last_source_update = iniGetIntValue(NULL, \
                STAT_ITEM_LAST_SOURCE_UPD, \
                &iniContext, 0);
        g_storage_stat.last_sync_update = iniGetIntValue(NULL, \
                STAT_ITEM_LAST_SYNC_UPD, \
                &iniContext, 0);
        g_storage_stat.total_set_meta_count = iniGetInt64Value(NULL, \
                STAT_ITEM_TOTAL_SET_META, \
                &iniContext, 0);
        g_storage_stat.success_set_meta_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_SET_META, \
                &iniContext, 0);
        g_storage_stat.total_delete_count = iniGetInt64Value(NULL, \
                STAT_ITEM_TOTAL_DELETE, \
                &iniContext, 0);
        g_storage_stat.success_delete_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_DELETE, \
                &iniContext, 0);
        g_storage_stat.total_get_meta_count = iniGetInt64Value(NULL, \
                STAT_ITEM_TOTAL_GET_META, \
                &iniContext, 0);
        g_storage_stat.success_get_meta_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_GET_META, \
                &iniContext, 0);
        g_storage_stat.total_create_link_count = iniGetInt64Value(NULL, \
                STAT_ITEM_TOTAL_CREATE_LINK, \
                &iniContext, 0);
        g_storage_stat.success_create_link_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_CREATE_LINK, \
                &iniContext, 0);
        g_storage_stat.total_delete_link_count = iniGetInt64Value(NULL, \
                STAT_ITEM_TOTAL_DELETE_LINK, \
                &iniContext, 0);
        g_storage_stat.success_delete_link_count = iniGetInt64Value(NULL, \
                STAT_ITEM_SUCCESS_DELETE_LINK, \
                &iniContext, 0);

        g_dist_path_index_high = iniGetIntValue(NULL, \
                STAT_ITEM_DIST_PATH_INDEX_HIGH, \
                &iniContext, 0);
        g_dist_path_index_low = iniGetIntValue(NULL, \
                STAT_ITEM_DIST_PATH_INDEX_LOW, \
                &iniContext, 0);
        g_dist_write_file_count = iniGetIntValue(NULL, \
                STAT_ITEM_DIST_WRITE_FILE_COUNT, \
                &iniContext, 0);

        iniFreeContext(&iniContext);
    }
    /*不存在该文件*/
    else
    {
        memset(&g_storage_stat, 0, sizeof(g_storage_stat));
    }

    storage_stat_fd = open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (storage_stat_fd < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "open stat file \"%s\" fail, " \
                "error no: %d, error info: %s", \
                __LINE__, full_filename, errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    /*读取无值会赋默认值，重新写入一次*/
    return storage_write_to_stat_file();
}

static int storage_close_stat_file()
{
    int result;

    result = 0;
    if (storage_stat_fd >= 0)
    {
        /*更新*/
        result = storage_write_to_stat_file();
        if (close(storage_stat_fd) != 0)
        {
            result += errno != 0 ? errno : ENOENT;
        }
        storage_stat_fd = -1;
    }

    return result;
}

int storage_write_to_stat_file()
{
    char buff[1024];
    int len;
    int result;
    int write_ret;

    /*格式化到buffer*/
    len = sprintf(buff, 
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s=%d\n"  \
            "%s=%d\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s="INT64_PRINTF_FORMAT"\n"  \
            "%s=%d\n"  \
            "%s=%d\n"  \
            "%s=%d\n", \
            STAT_ITEM_TOTAL_UPLOAD, g_storage_stat.total_upload_count, \
            STAT_ITEM_SUCCESS_UPLOAD, g_storage_stat.success_upload_count, \
            STAT_ITEM_TOTAL_DOWNLOAD, g_storage_stat.total_download_count, \
            STAT_ITEM_SUCCESS_DOWNLOAD, \
            g_storage_stat.success_download_count, \
            STAT_ITEM_LAST_SOURCE_UPD, \
            (int)g_storage_stat.last_source_update, \
            STAT_ITEM_LAST_SYNC_UPD, (int)g_storage_stat.last_sync_update,\
            STAT_ITEM_TOTAL_SET_META, g_storage_stat.total_set_meta_count, \
            STAT_ITEM_SUCCESS_SET_META, \
            g_storage_stat.success_set_meta_count, \
            STAT_ITEM_TOTAL_DELETE, g_storage_stat.total_delete_count, \
            STAT_ITEM_SUCCESS_DELETE, g_storage_stat.success_delete_count, \
            STAT_ITEM_TOTAL_GET_META, g_storage_stat.total_get_meta_count, \
            STAT_ITEM_SUCCESS_GET_META, \
            g_storage_stat.success_get_meta_count,  \
            STAT_ITEM_TOTAL_CREATE_LINK, \
            g_storage_stat.total_create_link_count,  \
            STAT_ITEM_SUCCESS_CREATE_LINK, \
            g_storage_stat.success_create_link_count,  \
            STAT_ITEM_TOTAL_DELETE_LINK, \
            g_storage_stat.total_delete_link_count,  \
            STAT_ITEM_SUCCESS_DELETE_LINK, \
            g_storage_stat.success_delete_link_count,  \
            STAT_ITEM_DIST_PATH_INDEX_HIGH, g_dist_path_index_high, \
            STAT_ITEM_DIST_PATH_INDEX_LOW, g_dist_path_index_low, \
            STAT_ITEM_DIST_WRITE_FILE_COUNT, g_dist_write_file_count
            );

    if ((result=pthread_mutex_lock(&sync_stat_file_lock)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_lock fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
    }

    /*写入*/
    write_ret = storage_write_to_fd(storage_stat_fd, \
            get_storage_stat_filename, NULL, buff, len);

    if ((result=pthread_mutex_unlock(&sync_stat_file_lock)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_unlock fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
    }

    return write_ret;
}

int storage_write_to_sync_ini_file()
{
    char full_filename[MAX_PATH_SIZE];
    char buff[512];
    int fd;
    int len;

    snprintf(full_filename, sizeof(full_filename), \
            "%s/data/%s", g_fdfs_base_path, DATA_DIR_INITED_FILENAME);
    if ((fd=open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "open file \"%s\" fail, " \
                "errno: %d, error info: %s", \
                __LINE__, full_filename, \
                errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    len = sprintf(buff, "%s=%d\n" \
            "%s=%d\n"  \
            "%s=%s\n"  \
            "%s=%d\n"  \
            "%s=%s\n"  \
            "%s=%d\n"  \
            "%s=%d\n", \
            INIT_ITEM_STORAGE_JOIN_TIME, g_storage_join_time, \
            INIT_ITEM_SYNC_OLD_DONE, g_sync_old_done, \
            INIT_ITEM_SYNC_SRC_SERVER, g_sync_src_ip_addr, \
            INIT_ITEM_SYNC_UNTIL_TIMESTAMP, g_sync_until_timestamp, \
            INIT_ITEM_LAST_IP_ADDRESS, g_tracker_client_ip, \
            INIT_ITEM_LAST_SERVER_PORT, g_last_server_port, \
            INIT_ITEM_LAST_HTTP_PORT, g_last_http_port
            );
    if (write(fd, buff, len) != len)
    {
        logError("file: "__FILE__", line: %d, " \
                "write to file \"%s\" fail, " \
                "errno: %d, error info: %s", \
                __LINE__, full_filename, \
                errno, strerror(errno));
        close(fd);
        return errno != 0 ? errno : EIO;
    }

    close(fd);
    return 0;
}

static int storage_check_and_make_data_dirs()
{
    int result;
    int i;
    char data_path[MAX_PATH_SIZE];
    char full_filename[MAX_PATH_SIZE];

    snprintf(data_path, sizeof(data_path), "%s/data", \
            g_fdfs_base_path);
    snprintf(full_filename, sizeof(full_filename), "%s/%s", \
            data_path, DATA_DIR_INITED_FILENAME);

    /*检查.data_init_flag文件是否存在，首次启动是不存在的 */
    if (fileExists(full_filename))
    {
        IniContext iniContext;
        char *pValue;

        /*从.data_init_flag加载数据*/
        if ((result=iniLoadFromFile(full_filename, &iniContext)) != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "load from file \"%s/%s\" fail, " \
                    "error code: %d", \
                    __LINE__, data_path, \
                    full_filename, result);
            return result;
        }

        pValue = iniGetStrValue(NULL, INIT_ITEM_STORAGE_JOIN_TIME, \
                &iniContext);
        if (pValue == NULL)
        {
            iniFreeContext(&iniContext);
            logError("file: "__FILE__", line: %d, " \
                    "in file \"%s/%s\", item \"%s\" not exists", \
                    __LINE__, data_path, full_filename, \
                    INIT_ITEM_STORAGE_JOIN_TIME);
            return ENOENT;
        }
        g_storage_join_time = atoi(pValue);

        pValue = iniGetStrValue(NULL, INIT_ITEM_SYNC_OLD_DONE, \
                &iniContext);
        if (pValue == NULL)
        {
            iniFreeContext(&iniContext);
            logError("file: "__FILE__", line: %d, " \
                    "in file \"%s/%s\", item \"%s\" not exists", \
                    __LINE__, data_path, full_filename, \
                    INIT_ITEM_SYNC_OLD_DONE);
            return ENOENT;
        }
        g_sync_old_done = atoi(pValue);

        pValue = iniGetStrValue(NULL, INIT_ITEM_SYNC_SRC_SERVER, \
                &iniContext);
        if (pValue == NULL)
        {
            iniFreeContext(&iniContext);
            logError("file: "__FILE__", line: %d, " \
                    "in file \"%s/%s\", item \"%s\" not exists", \
                    __LINE__, data_path, full_filename, \
                    INIT_ITEM_SYNC_SRC_SERVER);
            return ENOENT;
        }
        snprintf(g_sync_src_ip_addr, sizeof(g_sync_src_ip_addr), \
                "%s", pValue);

        g_sync_until_timestamp = iniGetIntValue(NULL, \
                INIT_ITEM_SYNC_UNTIL_TIMESTAMP, \
                &iniContext, 0);

        pValue = iniGetStrValue(NULL, INIT_ITEM_LAST_IP_ADDRESS, \
                &iniContext);
        if (pValue != NULL)
        {
            snprintf(g_last_storage_ip, sizeof(g_last_storage_ip), \
                    "%s", pValue);
        }

        pValue = iniGetStrValue(NULL, INIT_ITEM_LAST_SERVER_PORT, \
                &iniContext);
        if (pValue != NULL)
        {
            g_last_server_port = atoi(pValue);
        }

        pValue = iniGetStrValue(NULL, INIT_ITEM_LAST_HTTP_PORT, \
                &iniContext);
        if (pValue != NULL)
        {
            g_last_http_port = atoi(pValue);
        }

        iniFreeContext(&iniContext);

        if (g_last_server_port == 0 || g_last_http_port == 0)
        {
            if (g_last_server_port == 0)
            {
                g_last_server_port = g_server_port;
            }

            if (g_last_http_port == 0)
            {
                g_last_http_port = g_http_port;
            }

            if ((result=storage_write_to_sync_ini_file()) != 0)
            {
                return result;
            }
        }

        /*
           printf("g_sync_old_done = %d\n", g_sync_old_done);
           printf("g_sync_src_ip_addr = %s\n", g_sync_src_ip_addr);
           printf("g_sync_until_timestamp = %d\n", g_sync_until_timestamp);
           printf("g_last_storage_ip= %s\n", g_last_storage_ip);
           printf("g_last_server_port= %d\n", g_last_server_port);
           printf("g_last_http_port= %d\n", g_last_http_port);
           */
    } // 不存在
    else
    {
        if (!fileExists(data_path))
        {
            if (mkdir(data_path, 0755) != 0)
            {
                logError("file: "__FILE__", line: %d, " \
                        "mkdir \"%s\" fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, data_path, \
                        errno, strerror(errno));
                return errno != 0 ? errno : EPERM;
            }
        }

        g_last_server_port = g_server_port;
        g_last_http_port = g_http_port;
        g_storage_join_time = time(NULL);
        if ((result=storage_write_to_sync_ini_file()) != 0)
        {
            return result;
        }
    }

    for (i=0; i<g_path_count; i++)
    {
        /*创建数据目录*/
        if ((result=storage_make_data_dirs(g_store_paths[i])) != 0)
        {
            return result;
        }
    }

    return 0;
}

static int storage_make_data_dirs(const char *pBasePath)
{
    char data_path[MAX_PATH_SIZE];
    char dir_name[9];
    char sub_name[9];
    char min_sub_path[16];
    char max_sub_path[16];
    int i, k;

    snprintf(data_path, sizeof(data_path), "%s/data", pBasePath);
    if (!fileExists(data_path))
    {
        if (mkdir(data_path, 0755) != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "mkdir \"%s\" fail, " \
                    "errno: %d, error info: %s", \
                    __LINE__, data_path, errno, strerror(errno));
            return errno != 0 ? errno : EPERM;
        }
    }

    if (chdir(data_path) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "chdir \"%s\" fail, " \
                "errno: %d, error info: %s", \
                __LINE__, data_path, errno, strerror(errno));
        return errno != 0 ? errno : ENOENT;
    }

    sprintf(min_sub_path, STORAGE_DATA_DIR_FORMAT"/"STORAGE_DATA_DIR_FORMAT,
            0, 0);
    sprintf(max_sub_path, STORAGE_DATA_DIR_FORMAT"/"STORAGE_DATA_DIR_FORMAT,
            g_subdir_count_per_path-1, g_subdir_count_per_path-1);
    if (fileExists(min_sub_path) && fileExists(max_sub_path))
    {
        return 0;
    }

    fprintf(stderr, "data path: %s, mkdir sub dir...\n", data_path);
    for (i=0; i<g_subdir_count_per_path; i++)
    {
        sprintf(dir_name, STORAGE_DATA_DIR_FORMAT, i);

        fprintf(stderr, "mkdir data path: %s ...\n", dir_name);
        if (mkdir(dir_name, 0755) != 0)
        {
            if (!(errno == EEXIST && isDir(dir_name)))
            {
                logError("file: "__FILE__", line: %d, " \
                        "mkdir \"%s/%s\" fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, data_path, dir_name, \
                        errno, strerror(errno));
                return errno != 0 ? errno : ENOENT;
            }
        }

        if (chdir(dir_name) != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "chdir \"%s/%s\" fail, " \
                    "errno: %d, error info: %s", \
                    __LINE__, data_path, dir_name, \
                    errno, strerror(errno));
            return errno != 0 ? errno : ENOENT;
        }

        for (k=0; k<g_subdir_count_per_path; k++)
        {
            sprintf(sub_name, STORAGE_DATA_DIR_FORMAT, k);
            if (mkdir(sub_name, 0755) != 0)
            {
                if (!(errno == EEXIST && isDir(sub_name)))
                {
                    logError("file: "__FILE__", line: %d," \
                            " mkdir \"%s/%s/%s\" fail, " \
                            "errno: %d, error info: %s", \
                            __LINE__, data_path, \
                            dir_name, sub_name, \
                            errno, strerror(errno));
                    return errno != 0 ? errno : ENOENT;
                }
            }
        }

        if (chdir("..") != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "chdir \"%s\" fail, " \
                    "errno: %d, error info: %s", \
                    __LINE__, data_path, \
                    errno, strerror(errno));
            return errno != 0 ? errno : ENOENT;
        }
    }

    fprintf(stderr, "data path: %s, mkdir sub dir done.\n", data_path);

    return 0;
}

/*
   static int init_fsync_pthread_cond()
   {
   int result;
   pthread_condattr_t thread_condattr;
   if ((result=pthread_condattr_init(&thread_condattr)) != 0)
   {
   logError("file: "__FILE__", line: %d, " \
   "pthread_condattr_init failed, " \
   "errno: %d, error info: %s", \
   __LINE__, result, strerror(result));
   return result;
   }

   if ((result=pthread_cond_init(&fsync_thread_cond, &thread_condattr)) != 0)
   {
   logError("file: "__FILE__", line: %d, " \
   "pthread_cond_init failed, " \
   "errno: %d, error info: %s", \
   __LINE__, result, strerror(result));
   return result;
   }

   pthread_condattr_destroy(&thread_condattr);
   return 0;
   }
   */

static int storage_load_paths(IniContext *pItemContext)
{
    char item_name[64];
    char *pPath;
    int i;

    pPath = iniGetStrValue(NULL, "base_path", pItemContext);
    if (pPath == NULL)
    {
        logError("file: "__FILE__", line: %d, " \
                "conf file must have item \"base_path\"!", __LINE__);
        return ENOENT;
    }

    snprintf(g_fdfs_base_path, sizeof(g_fdfs_base_path), "%s", pPath);
    chopPath(g_fdfs_base_path);
    if (!fileExists(g_fdfs_base_path))
    {
        logError("file: "__FILE__", line: %d, " \
                "\"%s\" can't be accessed, error info: %s", \
                __LINE__, strerror(errno), g_fdfs_base_path);
        return errno != 0 ? errno : ENOENT;
    }
    if (!isDir(g_fdfs_base_path))
    {
        logError("file: "__FILE__", line: %d, " \
                "\"%s\" is not a directory!", \
                __LINE__, g_fdfs_base_path);
        return ENOTDIR;
    }

    g_path_count = iniGetIntValue(NULL, "store_path_count", pItemContext,1);
    if (g_path_count <= 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "store_path_count: %d is invalid!", \
                __LINE__, g_path_count);
        return EINVAL;
    }

    g_store_paths = (char **)malloc(sizeof(char *) * g_path_count);
    if (g_store_paths == NULL)
    {
        logError("file: "__FILE__", line: %d, " \
                "malloc %d bytes fail, errno: %d, error info: %s", \
                __LINE__, (int)sizeof(char *) *g_path_count, \
                errno, strerror(errno));
        return errno != 0 ? errno : ENOMEM;
    }
    memset(g_store_paths, 0, sizeof(char *) * g_path_count);

    pPath = iniGetStrValue(NULL, "store_path0", pItemContext);
    if (pPath == NULL)
    {
        pPath = g_fdfs_base_path;
    }
    g_store_paths[0] = strdup(pPath);
    if (g_store_paths[0] == NULL)
    {
        logError("file: "__FILE__", line: %d, " \
                "malloc %d bytes fail, errno: %d, error info: %s", \
                __LINE__, (int)strlen(pPath), errno, strerror(errno));
        return errno != 0 ? errno : ENOMEM;
    }

    for (i=1; i<g_path_count; i++)
    {
        sprintf(item_name, "store_path%d", i);
        pPath = iniGetStrValue(NULL, item_name, pItemContext);
        if (pPath == NULL)
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file must have item \"%s\"!", \
                    __LINE__, item_name);
            return ENOENT;
        }

        chopPath(pPath);
        if (!fileExists(pPath))
        {
            logError("file: "__FILE__", line: %d, " \
                    "\"%s\" can't be accessed, error info: %s", \
                    __LINE__, strerror(errno), pPath);
            return errno != 0 ? errno : ENOENT;
        }
        if (!isDir(pPath))
        {
            logError("file: "__FILE__", line: %d, " \
                    "\"%s\" is not a directory!", \
                    __LINE__, pPath);
            return ENOTDIR;
        }

        g_store_paths[i] = strdup(pPath);
        if (g_store_paths[i] == NULL)
        {
            logError("file: "__FILE__", line: %d, " \
                    "malloc %d bytes fail, " \
                    "errno: %d, error info: %s", __LINE__, \
                    (int)strlen(pPath), errno, strerror(errno));
            return errno != 0 ? errno : ENOMEM;
        }
    }

    return 0;
}

/*读取配置，检查ip变化，获取changelog，读取state文件*/
int storage_func_init(const char *filename, \
        char *bind_addr, const int addr_size)
{
    char *pBindAddr;
    char *pGroupName;
    char *pRunByGroup;
    char *pRunByUser;
    char *pFsyncAfterWrittenBytes;
    char *pThreadStackSize;
    char *pIfAliasPrefix;
    char *pHttpDomain;
    IniContext iniContext;
    int result;
    int64_t fsync_after_written_bytes;
    int64_t thread_stack_size;

    /*
       while (nThreadCount > 0)
       {
       sleep(1);
       }
       */

    /*处理配置文件*/
    if ((result=iniLoadFromFile(filename, &iniContext)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "load conf file \"%s\" fail, ret code: %d", \
                __LINE__, filename, result);
        return result;
    }

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

        g_subdir_count_per_path=iniGetIntValue(NULL, \
                "subdir_count_per_path", &iniContext, \
                DEFAULT_DATA_DIR_COUNT_PER_PATH);
        if (g_subdir_count_per_path <= 0 || \
                g_subdir_count_per_path > 256)
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file \"%s\", invalid subdir_count: %d", \
                    __LINE__, filename, g_subdir_count_per_path);
            result = EINVAL;
            break;
        }

        if ((result=storage_load_paths(&iniContext)) != 0)
        {
            break;
        }

        load_log_level(&iniContext);
        if ((result=log_set_prefix(g_fdfs_base_path, \
                        STORAGE_ERROR_LOG_FILENAME)) != 0)
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
                FDFS_STORAGE_SERVER_DEF_PORT);
        if (g_server_port <= 0)
        {
            g_server_port = FDFS_STORAGE_SERVER_DEF_PORT;
        }

        g_heart_beat_interval = iniGetIntValue(NULL, \
                "heart_beat_interval", &iniContext, \
                STORAGE_BEAT_DEF_INTERVAL);
        if (g_heart_beat_interval <= 0)
        {
            g_heart_beat_interval = STORAGE_BEAT_DEF_INTERVAL;
        }

        g_stat_report_interval = iniGetIntValue(NULL, \
                "stat_report_interval", &iniContext, \
                STORAGE_REPORT_DEF_INTERVAL);
        if (g_stat_report_interval <= 0)
        {
            g_stat_report_interval = STORAGE_REPORT_DEF_INTERVAL;
        }

        pBindAddr = iniGetStrValue(NULL, "bind_addr", &iniContext);
        if (pBindAddr == NULL)
        {
            *bind_addr = '\0';
        }
        else
        {
            snprintf(bind_addr, addr_size, "%s", pBindAddr);
        }

        g_client_bind_addr = iniGetBoolValue(NULL, "client_bind", \
                &iniContext, true);

        pGroupName = iniGetStrValue(NULL, "group_name", &iniContext);
        if (pGroupName == NULL)
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file \"%s\" must have item " \
                    "\"group_name\"!", \
                    __LINE__, filename);
            result = ENOENT;
            break;
        }
        if (pGroupName[0] == '\0')
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file \"%s\", " \
                    "group_name is empty!", \
                    __LINE__, filename);
            result = EINVAL;
            break;
        }

        snprintf(g_group_name, sizeof(g_group_name), "%s", pGroupName);
        /*验证组名是否合法*/
        if ((result=fdfs_validate_group_name(g_group_name)) != 0) \
        {
            logError("file: "__FILE__", line: %d, " \
                    "conf file \"%s\", " \
                    "the group name \"%s\" is invalid!", \
                    __LINE__, filename, g_group_name);
            result = EINVAL;
            break;
        }

        /*加载Tracker group*/
        result = fdfs_load_tracker_group_ex(&g_tracker_group, \
                filename, &iniContext);
        if (result != 0)
        {
            break;
        }

        g_sync_wait_usec = iniGetIntValue(NULL, "sync_wait_msec",\
                &iniContext, STORAGE_DEF_SYNC_WAIT_MSEC);
        if (g_sync_wait_usec <= 0)
        {
            g_sync_wait_usec = STORAGE_DEF_SYNC_WAIT_MSEC;
        }
        g_sync_wait_usec *= 1000;

        g_sync_interval = iniGetIntValue(NULL, "sync_interval",\
                &iniContext, 0);
        if (g_sync_interval < 0)
        {
            g_sync_interval = 0;
        }
        g_sync_interval *= 1000;

        if ((result=get_time_item_from_conf(&iniContext, \
                        "sync_start_time", &g_sync_start_time, 0, 0)) != 0)
        {
            break;
        }
        if ((result=get_time_item_from_conf(&iniContext, \
                        "sync_end_time", &g_sync_end_time, 23, 59)) != 0)
        {
            break;
        }

        g_sync_part_time = !((g_sync_start_time.hour == 0 && \
                    g_sync_start_time.minute == 0) && \
                (g_sync_end_time.hour == 23 && \
                 g_sync_end_time.minute == 59));

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

        g_file_distribute_path_mode = iniGetIntValue(NULL, \
                "file_distribute_path_mode", &iniContext, \
                FDFS_FILE_DIST_PATH_ROUND_ROBIN);
        g_file_distribute_rotate_count = iniGetIntValue(NULL, \
                "file_distribute_rotate_count", &iniContext, \
                FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT);
        if (g_file_distribute_rotate_count <= 0)
        {
            g_file_distribute_rotate_count = \
                                             FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT;
        }

        pFsyncAfterWrittenBytes = iniGetStrValue(NULL, \
                "fsync_after_written_bytes", &iniContext);
        if (pFsyncAfterWrittenBytes == NULL)
        {
            fsync_after_written_bytes = 0;
        }
        else if ((result=parse_bytes(pFsyncAfterWrittenBytes, 1, \
                        &fsync_after_written_bytes)) != 0)
        {
            return result;
        }
        g_fsync_after_written_bytes = fsync_after_written_bytes;

        g_sync_log_buff_interval = iniGetIntValue(NULL, \
                "sync_log_buff_interval", &iniContext, \
                SYNC_LOG_BUFF_DEF_INTERVAL);
        if (g_sync_log_buff_interval <= 0)
        {
            g_sync_log_buff_interval = SYNC_LOG_BUFF_DEF_INTERVAL;
        }

        g_sync_binlog_buff_interval = iniGetIntValue(NULL, \
                "sync_binlog_buff_interval", &iniContext,\
                SYNC_BINLOG_BUFF_DEF_INTERVAL);
        if (g_sync_binlog_buff_interval <= 0)
        {
            g_sync_binlog_buff_interval=SYNC_BINLOG_BUFF_DEF_INTERVAL;
        }

        g_write_mark_file_freq = iniGetIntValue(NULL, \
                "write_mark_file_freq", &iniContext, \
                DEFAULT_SYNC_MARK_FILE_FREQ);
        if (g_write_mark_file_freq <= 0)
        {
            g_write_mark_file_freq = DEFAULT_SYNC_MARK_FILE_FREQ;
        }


        g_sync_stat_file_interval = iniGetIntValue(NULL, \
                "sync_stat_file_interval", &iniContext, \
                DEFAULT_SYNC_STAT_FILE_INTERVAL);
        if (g_sync_stat_file_interval <= 0)
        {
            g_sync_stat_file_interval=DEFAULT_SYNC_STAT_FILE_INTERVAL;
        }

        pThreadStackSize = iniGetStrValue(NULL, \
                "thread_stack_size", &iniContext);
        if (pThreadStackSize == NULL)
        {
            thread_stack_size = 512 * 1024;
        }
        else if ((result=parse_bytes(pThreadStackSize, 1, \
                        &thread_stack_size)) != 0)
        {
            return result;
        }
        g_thread_stack_size = (int)thread_stack_size;

        if (g_thread_stack_size < FDFS_WRITE_BUFF_SIZE + 64 * 1024)
        {
            logError("file: "__FILE__", line: %d, " \
                    "item \"thread_stack_size\" %d is invalid, " \
                    "which < %d", __LINE__, g_thread_stack_size, \
                    FDFS_WRITE_BUFF_SIZE + 64 * 1024);
            result = EINVAL;
            break;
        }

        g_upload_priority = iniGetIntValue(NULL, \
                "upload_priority", &iniContext, \
                DEFAULT_UPLOAD_PRIORITY);

        pIfAliasPrefix = iniGetStrValue(NULL, \
                "if_alias_prefix", &iniContext);
        if (pIfAliasPrefix == NULL)
        {
            *g_if_alias_prefix = '\0';
        }
        else
        {
            snprintf(g_if_alias_prefix, sizeof(g_if_alias_prefix), 
                    "%s", pIfAliasPrefix);
        }

        g_check_file_duplicate = iniGetBoolValue(NULL, \
                "check_file_duplicate", &iniContext, false);
        if (g_check_file_duplicate)
        {
            char *pKeyNamespace;

            strcpy(g_fdht_base_path, g_fdfs_base_path);
            g_fdht_network_timeout = g_fdfs_network_timeout;

            pKeyNamespace = iniGetStrValue(NULL, \
                    "key_namespace", &iniContext);
            if (pKeyNamespace == NULL || *pKeyNamespace == '\0')
            {
                logError("file: "__FILE__", line: %d, " \
                        "item \"key_namespace\" does not " \
                        "exist or is empty", __LINE__);
                result = EINVAL;
                break;
            }

            g_namespace_len = strlen(pKeyNamespace);
            if (g_namespace_len >= sizeof(g_key_namespace))
            {
                g_namespace_len = sizeof(g_key_namespace) - 1;
            }
            memcpy(g_key_namespace, pKeyNamespace, g_namespace_len);
            *(g_key_namespace + g_namespace_len) = '\0';

            if ((result=fdht_load_groups(&iniContext, \
                            &g_group_array)) != 0)
            {
                break;
            }

            g_keep_alive = iniGetBoolValue(NULL, "keep_alive", \
                    &iniContext, false);
        }

        g_http_port = iniGetIntValue(NULL, "http.server_port", \
                &iniContext, 80);
        if (g_http_port <= 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "invalid param \"http.server_port\": %d", \
                    __LINE__, g_http_port);
            return EINVAL;
        }

        pHttpDomain = iniGetStrValue(NULL, \
                "http.domain_name", &iniContext);
        if (pHttpDomain == NULL)
        {
            *g_http_domain = '\0';
        }
        else
        {
            snprintf(g_http_domain, sizeof(g_http_domain), \
                    "%s", pHttpDomain);
        }

#ifdef WITH_HTTPD
        {
            char *pHttpTrunkSize;
            int64_t http_trunk_size;

            if ((result=fdfs_http_params_load(&iniContext, \
                            filename, &g_http_params)) != 0)
            {
                break;
            }

            pHttpTrunkSize = iniGetStrValue(NULL, \
                    "http.trunk_size", &iniContext);
            if (pHttpTrunkSize == NULL)
            {
                http_trunk_size = 64 * 1024;
            }
            else if ((result=parse_bytes(pHttpTrunkSize, 1, \
                            &http_trunk_size)) != 0)
            {
                break;
            }

            g_http_trunk_size = (int)http_trunk_size;
        }
#endif

        logInfo("FastDFS v%d.%d, base_path=%s, store_path_count=%d, " \
                "subdir_count_per_path=%d, group_name=%s, " \
                "connect_timeout=%ds, network_timeout=%ds, "\
                "port=%d, bind_addr=%s, client_bind=%d, " \
                "max_connections=%d, "    \
                "heart_beat_interval=%ds, " \
                "stat_report_interval=%ds, tracker_server_count=%d, " \
                "sync_wait_msec=%dms, sync_interval=%dms, " \
                "sync_start_time=%02d:%02d, sync_end_time: %02d:%02d, "\
                "write_mark_file_freq=%d, " \
                "allow_ip_count=%d, " \
                "file_distribute_path_mode=%d, " \
                "file_distribute_rotate_count=%d, " \
                "fsync_after_written_bytes=%d, " \
                "sync_log_buff_interval=%ds, " \
                "sync_binlog_buff_interval=%ds, " \
                "sync_stat_file_interval=%ds, " \
                "thread_stack_size=%d KB, upload_priority=%d, " \
                "if_alias_prefix=%s, " \
                "check_file_duplicate=%d, FDHT group count=%d, " \
                "FDHT server count=%d, FDHT key_namespace=%s, " \
                "FDHT keep_alive=%d, HTTP server port=%d, " \
                "domain name=%s", \
                g_fdfs_version.major, g_fdfs_version.minor, \
                g_fdfs_base_path, g_path_count, g_subdir_count_per_path,\
                g_group_name, g_fdfs_connect_timeout, \
                g_fdfs_network_timeout, g_server_port, bind_addr, \
                g_client_bind_addr, g_max_connections, \
                g_heart_beat_interval, g_stat_report_interval, \
                g_tracker_group.server_count, g_sync_wait_usec / 1000, \
                g_sync_interval / 1000, \
                g_sync_start_time.hour, g_sync_start_time.minute, \
                g_sync_end_time.hour, g_sync_end_time.minute, \
                g_write_mark_file_freq, \
                g_allow_ip_count, g_file_distribute_path_mode, \
                g_file_distribute_rotate_count, \
                g_fsync_after_written_bytes, g_sync_log_buff_interval, \
                g_sync_binlog_buff_interval, g_sync_stat_file_interval, \
                g_thread_stack_size/1024, g_upload_priority, \
                g_if_alias_prefix, g_check_file_duplicate, \
                g_group_array.group_count, g_group_array.server_count, \
                g_key_namespace, g_keep_alive, \
                g_http_port, g_http_domain);

#ifdef WITH_HTTPD
        if (!g_http_params.disabled)
        {
            logInfo("HTTP supported: " \
                    "server_port=%d, " \
                    "http_trunk_size=%d, " \
                    "default_content_type=%s, " \
                    "anti_steal_token=%d, " \
                    "token_ttl=%ds, " \
                    "anti_steal_secret_key length=%d, "  \
                    "token_check_fail content_type=%s, " \
                    "token_check_fail buff length=%d",  \
                    g_http_params.server_port, \
                    g_http_trunk_size, \
                    g_http_params.default_content_type, \
                    g_http_params.anti_steal_token, \
                    g_http_params.token_ttl, \
                    g_http_params.anti_steal_secret_key.length, \
                    g_http_params.token_check_fail_content_type, \
                    g_http_params.token_check_fail_buff.length);
        }
#endif

    } while (0);

    /*解析完，释放空间*/
    iniFreeContext(&iniContext);

    if (result != 0)
    {
        return result;
    }

    /*加载.data_init_flag, 创建目录*/
    if ((result=storage_check_and_make_data_dirs()) != 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "storage_check_and_make_data_dirs fail, " \
                "program exit!", __LINE__);
        return result;
    }

    /*从Tracker中拿一个参数：是否自动调整ip*/
    if ((result=storage_get_params_from_tracker()) != 0)
    {
        return result;
    }

    /*
     *1，自动调整：
     *连接所有的Tracker，通过getsockip来获取此次连接自己的ip
     *然后检查ip是否发生了改变，如果改变了，发送ip_changed的报告，
     *那么服务器对应本Storage被清空，然后设为本机地址,并且修改changelog,
     *接着再发请求，把changelog拿回来,然后重命名ip_port.mark文件
     *2，不自动调整，不处理
     */
    if ((result=storage_check_ip_changed()) != 0)
    {
        return result;
    }

    if ((result=init_pthread_lock(&sync_stat_file_lock)) != 0)
    {
        return result;
    }

    /*读取storage_stat.dat*/
    return storage_open_stat_file();
}

int storage_func_destroy()
{
    int i;
    int result;
    int close_ret;

    if (g_store_paths != NULL)
    {
        for (i=0; i<g_path_count; i++)
        {
            if (g_store_paths[i] != NULL)
            {
                free(g_store_paths[i]);
                g_store_paths[i] = NULL;
            }
        }

        g_store_paths = NULL;
    }

    if (g_tracker_group.servers != NULL)
    {
        free(g_tracker_group.servers);
        g_tracker_group.servers = NULL;
        g_tracker_group.server_count = 0;
        g_tracker_group.server_index = 0;
    }

    close_ret = storage_close_stat_file();

    if ((result=pthread_mutex_destroy(&sync_stat_file_lock)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_destroy fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    return close_ret;
}

#define SPLIT_FILENAME_BODY(logic_filename, \
        filename_len, true_filename, store_path_index) \
char buff[3]; \
char *pEnd; \
\
do \
{ \
    if (*filename_len <= FDFS_FILE_PATH_LEN) \
    { \
        logError("file: "__FILE__", line: %d, " \
                "filename_len: %d is invalid, <= %d", \
                __LINE__, *filename_len, FDFS_FILE_PATH_LEN); \
        return EINVAL; \
    } \
    \
    if (*logic_filename != STORAGE_STORE_PATH_PREFIX_CHAR) \
    { /* version < V1.12 */ \
        store_path_index = 0; \
        memcpy(true_filename, logic_filename, (*filename_len)+1); \
        break; \
    } \
    \
    if (*(logic_filename + 3) != '/') \
    { \
        logError("file: "__FILE__", line: %d, " \
                "filename: %s is invalid", \
                __LINE__, logic_filename); \
        return EINVAL; \
    } \
    \
    *buff = *(logic_filename+1); \
    *(buff+1) = *(logic_filename+2); \
    *(buff+2) = '\0'; \
    \
    pEnd = NULL; \
    store_path_index = strtol(buff, &pEnd, 16); \
    if (pEnd != NULL && *pEnd != '\0') \
    { \
        logError("file: "__FILE__", line: %d, " \
                "filename: %s is invalid", \
                __LINE__, logic_filename); \
        return EINVAL; \
    } \
    \
    if (store_path_index < 0 || store_path_index >= g_path_count) \
    { \
        logError("file: "__FILE__", line: %d, " \
                "filename: %s is invalid, " \
                "invalid store path index: %d", \
                __LINE__, logic_filename, store_path_index); \
        return EINVAL; \
    } \
    \
    *filename_len -= 4; \
    memcpy(true_filename, logic_filename + 4, (*filename_len) + 1); \
    \
} while (0);


int storage_split_filename(const char *logic_filename, \
        int *filename_len, char *true_filename, char **ppStorePath)
{
    int store_path_index;

    SPLIT_FILENAME_BODY(logic_filename, \
            filename_len, true_filename, store_path_index)

        *ppStorePath = g_store_paths[store_path_index];

    return 0;
}

int storage_split_filename_ex(const char *logic_filename, \
        int *filename_len, char *true_filename, int *store_path_index)
{
    SPLIT_FILENAME_BODY(logic_filename, \
            filename_len, true_filename, *store_path_index)

        return 0;
}

/*
   int write_serialized(int fd, const char *buff, size_t count, const bool bSync)
   {
   int result;
   int fsync_ret;

   if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
   {
   logError("file: "__FILE__", line: %d, " \
   "call pthread_mutex_lock fail, " \
   "errno: %d, error info: %s", \
   __LINE__, result, strerror(result));
   return result;
   }

   while (fsync_thread_count >= g_max_write_thread_count)
   {
   if ((result=pthread_cond_wait(&fsync_thread_cond, \
   &fsync_thread_mutex)) != 0)
   {
   logError("file: "__FILE__", line: %d, " \
   "pthread_cond_wait failed, " \
   "errno: %d, error info: %s", \
   __LINE__, result, strerror(result));
   return result;
   }
   }

   fsync_thread_count++;

   if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
   {
   logError("file: "__FILE__", line: %d, " \
   "call pthread_mutex_unlock fail, " \
   "errno: %d, error info: %s", \
   __LINE__, result, strerror(result));
   }

   if (write(fd, buff, count) == count)
   {
   if (bSync && fsync(fd) != 0)
   {
   fsync_ret = errno != 0 ? errno : EIO;
   logError("file: "__FILE__", line: %d, " \
   "call fsync fail, " \
   "errno: %d, error info: %s", \
   __LINE__, fsync_ret, strerror(fsync_ret));
   }
   else
   {
   fsync_ret = 0;
   }
   }
   else
   {
   fsync_ret = errno != 0 ? errno : EIO;
   logError("file: "__FILE__", line: %d, " \
   "call write fail, " \
   "errno: %d, error info: %s", \
   __LINE__, fsync_ret, strerror(fsync_ret));
   }

   if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
   {
   logError("file: "__FILE__", line: %d, " \
   "call pthread_mutex_lock fail, " \
   "errno: %d, error info: %s", \
   __LINE__, result, strerror(result));
   }

   fsync_thread_count--;

if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
{
    logError("file: "__FILE__", line: %d, " \
            "call pthread_mutex_unlock fail, " \
            "errno: %d, error info: %s", \
            __LINE__, result, strerror(result));
}

if ((result=pthread_cond_signal(&fsync_thread_cond)) != 0)
{
    logError("file: "__FILE__", line: %d, " \
            "pthread_cond_signal failed, " \
            "errno: %d, error info: %s", \
            __LINE__, result, strerror(result));
}

return fsync_ret;
}

int fsync_serialized(int fd)
{
    int result;
    int fsync_ret;

    if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_lock fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
        return result;
    }

    while (fsync_thread_count >= g_max_write_thread_count)
    {
        if ((result=pthread_cond_wait(&fsync_thread_cond, \
                        &fsync_thread_mutex)) != 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "pthread_cond_wait failed, " \
                    "errno: %d, error info: %s", \
                    __LINE__, result, strerror(result));
            return result;
        }
    }

    fsync_thread_count++;

    if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_unlock fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
    }

    if (fsync(fd) == 0)
    {
        fsync_ret = 0;
    }
    else
    {
        fsync_ret = errno != 0 ? errno : EIO;
        logError("file: "__FILE__", line: %d, " \
                "call fsync fail, " \
                "errno: %d, error info: %s", \
                __LINE__, fsync_ret, strerror(fsync_ret));
    }

    if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_lock fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
    }

    fsync_thread_count--;

    if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call pthread_mutex_unlock fail, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
    }

    if ((result=pthread_cond_signal(&fsync_thread_cond)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "pthread_cond_signal failed, " \
                "errno: %d, error info: %s", \
                __LINE__, result, strerror(result));
    }

    return fsync_ret;
}

int recv_file_serialized(int sock, const char *filename, \
        const int64_t file_bytes)
{
    int fd;
    char buff[FDFS_WRITE_BUFF_SIZE];
    int64_t remain_bytes;
    int recv_bytes;
    int result;

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    remain_bytes = file_bytes;
    while (remain_bytes > 0)
    {
        if (remain_bytes > sizeof(buff))
        {
            recv_bytes = sizeof(buff);
        }
        else
        {
            recv_bytes = remain_bytes;
        }

        if ((result=tcprecvdata_nb(sock, buff, recv_bytes, \
                        g_fdfs_network_timeout)) != 0)
        {
            close(fd);
            unlink(filename);
            return result;
        }

        if (recv_bytes == remain_bytes)  //last buff
        {
            if (write_serialized(fd, buff, recv_bytes, true) != 0)
            {
                result = errno != 0 ? errno : EIO;
                close(fd);
                unlink(filename);
                return result;
            }
        }
        else
        {
            if (write_serialized(fd, buff, recv_bytes, false) != 0)
            {
                result = errno != 0 ? errno : EIO;
                close(fd);
                unlink(filename);
                return result;
            }

            if ((result=fsync_serialized(fd)) != 0)
            {
                close(fd);
                unlink(filename);
                return result;
            }
        }

        remain_bytes -= recv_bytes;
    }

    close(fd);
    return 0;
}
*/

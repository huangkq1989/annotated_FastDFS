/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 该文件处理的事情很简单，只在调试的情况下才会
 * 起作用，在接收到SIGUSR1和SIGUSR2信号时，把
 * tracker的状态信息持久化到文件中，包括以下信息：
 * 1.首先是打印Tracker全局变量的信息
 * 2.然后打印描述所有组信息的信息
 * 3.最后打印各组的信息，在打印各组信息时，
 * 会继续打印组内各storage的信息
 *
 **************************************************/

/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "tracker_dump.h"
#include "shared_func.h"
#include "sched_thread.h"
#include "logger.h"
#include "hash.h"
#include "fdfs_global.h"
#include "tracker_global.h"
#include "tracker_mem.h"
#include "tracker_service.h"

static int fdfs_dump_storage_stat(FDFSStorageDetail *pServer, 
		char *buff, const int buffSize);

/*组的状态*/
static int fdfs_dump_group_stat(FDFSGroupInfo *pGroup, char *buff, const int buffSize)
{
	char szLastSourceUpdate[32];
	char szLastSyncUpdate[32];
	char szSyncedTimestamp[32];
	int total_len;
	FDFSStorageDetail *pServer;
	FDFSStorageDetail *pServerEnd;
	FDFSStorageDetail **ppServer;
	FDFSStorageDetail **ppServerEnd;
	int i;
	int j;

	total_len = snprintf(buff, buffSize, 
		"group_name=%s\n"
		"dirty=%d\n"
		"free_mb="INT64_PRINTF_FORMAT"\n"
		"alloc_size=%d\n"
		"server count=%d\n"
		"active server count=%d\n"
		"storage_port=%d\n"
		"storage_http_port=%d\n"
		"current_read_server=%d\n"
		"current_write_server=%d\n"
		"store_path_count=%d\n"
		"subdir_count_per_path=%d\n" 
		"pStoreServer=%s\n" 
		"ref_count=%d\n"
		"chg_count=%d\n"
		"last_source_update=%s\n"
		"last_sync_update=%s\n",
		pGroup->group_name, 
		pGroup->dirty, 
		pGroup->free_mb, 
		pGroup->alloc_size, 
		pGroup->count, 
		pGroup->active_count, 
		pGroup->storage_port,
		pGroup->storage_http_port, 
		pGroup->current_read_server, 
		pGroup->current_write_server, 
		pGroup->store_path_count,
		pGroup->subdir_count_per_path,
		pGroup->pStoreServer != NULL ? pGroup->pStoreServer->ip_addr : "",
		*(pGroup->ref_count),
		pGroup->chg_count,
		formatDatetime(pGroup->last_source_update, 
			"%Y-%m-%d %H:%M:%S", 
			szLastSourceUpdate, sizeof(szLastSourceUpdate)),
		formatDatetime(pGroup->last_sync_update, 
			"%Y-%m-%d %H:%M:%S", 
			szLastSyncUpdate, sizeof(szLastSyncUpdate))
	);


    /*所有的服务器及其各自的ip*/
	total_len += snprintf(buff + total_len, buffSize - total_len, 
		"total server count=%d\n", pGroup->count);
	pServerEnd = pGroup->all_servers + pGroup->count;
	for (pServer=pGroup->all_servers; pServer<pServerEnd; pServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len, 
			"\t%s\n", pServer->ip_addr);
	}

    /*激活的服务器及其各自的ip*/
	total_len += snprintf(buff + total_len, buffSize - total_len, 
		"\nactive server count=%d\n", pGroup->active_count);
	ppServerEnd = pGroup->active_servers + pGroup->active_count;
	for (ppServer=pGroup->active_servers; ppServer<ppServerEnd; ppServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len, 
			"\t%s\n", (*ppServer)->ip_addr);
	}

#ifdef WITH_HTTPD
	total_len += snprintf(buff + total_len, buffSize - total_len, 
		"\nhttp active server count=%d\n"
		"current_http_server=%d\n", 
		pGroup->http_server_count,
		pGroup->current_http_server);

	ppServerEnd = pGroup->http_servers + pGroup->http_server_count;
	for (ppServer=pGroup->http_servers; ppServer<ppServerEnd; ppServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len, 
			"\t%s\n", (*ppServer)->ip_addr);
	}
#endif

    /*根据ip排过序的storage*/
	ppServerEnd = pGroup->sorted_servers + pGroup->count;
	for (ppServer=pGroup->sorted_servers; ppServer<ppServerEnd; ppServer++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len, 
				"\nHost %d.\n", 
				(int)(ppServer - pGroup->sorted_servers) + 1);
		total_len += fdfs_dump_storage_stat(*ppServer, buff + total_len,
				 buffSize - total_len);
	}

    /*同步时间戳*/
    total_len += snprintf(buff + total_len, buffSize - total_len, 
            "\nsynced timestamp table:\n");
    for (i=0; i<pGroup->count; i++)
    {
        for (j=0; j<pGroup->count; j++)
        {
            if (i == j)
            {
                continue;
            }

            total_len += snprintf(buff + total_len, buffSize - total_len, 
                    "\t%s => %s: %s\n", 
                    pGroup->all_servers[i].ip_addr, 
                    pGroup->all_servers[j].ip_addr,
                    formatDatetime(pGroup->last_sync_timestamps[i][j],
                        "%Y-%m-%d %H:%M:%S", 
                        szSyncedTimestamp, 
                        sizeof(szSyncedTimestamp))
                    );
        }
    }

	total_len += snprintf(buff + total_len, buffSize - total_len, 
			"\n\n");
	return total_len;
}

/*storage的状态*/
static int fdfs_dump_storage_stat(FDFSStorageDetail *pServer, 
		char *buff, const int buffSize)
{
	char szUpTime[32];
	char szLastHeartBeatTime[32];
	char szSrcUpdTime[32];
	char szSyncUpdTime[32];
	char szSyncedTimestamp[32];
	char szSyncUntilTimestamp[32];
	int i;
	int total_len;

	total_len = snprintf(buff, buffSize, 
		"ip_addr=%s\n"
		"version=%s\n"
		"status=%d\n"
		"dirty=%d\n"
		"domain_name=%s\n"
		"sync_src_server=%s\n"
		"sync_until_timestamp=%s\n"
		"up_time=%s\n"
		"total_mb="INT64_PRINTF_FORMAT" MB\n"
		"free_mb="INT64_PRINTF_FORMAT" MB\n"
		"changelog_offset="INT64_PRINTF_FORMAT"\n"
		"store_path_count=%d\n"
		"storage_port=%d\n"
		"storage_http_port=%d\n"
		"subdir_count_per_path=%d\n"
		"upload_priority=%d\n"
		"current_write_path=%d\n"
		"ref_count=%d\n"
		"chg_count=%d\n"
#ifdef WITH_HTTPD
		"http_check_last_errno=%d\n"
		"http_check_last_status=%d\n"
		"http_check_fail_count=%d\n"
		"http_check_error_info=%s\n"
#endif

		"total_upload_count="INT64_PRINTF_FORMAT"\n"
		"success_upload_count="INT64_PRINTF_FORMAT"\n"
		"total_set_meta_count="INT64_PRINTF_FORMAT"\n"
		"success_set_meta_count="INT64_PRINTF_FORMAT"\n"
		"total_delete_count="INT64_PRINTF_FORMAT"\n"
		"success_delete_count="INT64_PRINTF_FORMAT"\n"
		"total_download_count="INT64_PRINTF_FORMAT"\n"
		"success_download_count="INT64_PRINTF_FORMAT"\n"
		"total_get_meta_count="INT64_PRINTF_FORMAT"\n"
		"success_get_meta_count="INT64_PRINTF_FORMAT"\n"
		"total_create_link_count="INT64_PRINTF_FORMAT"\n"
		"success_create_link_count="INT64_PRINTF_FORMAT"\n"
		"total_delete_link_count="INT64_PRINTF_FORMAT"\n"
		"success_delete_link_count="INT64_PRINTF_FORMAT"\n"
		"last_source_update=%s\n"
		"last_sync_update=%s\n"
		"last_synced_timestamp=%s\n"
		"last_heart_beat_time=%s\n",
		pServer->ip_addr, 
		pServer->version, 
		pServer->status, 
		pServer->dirty, 
		pServer->domain_name, 
		pServer->psync_src_server != NULL ? 
		pServer->psync_src_server->ip_addr : "", 
		formatDatetime(pServer->sync_until_timestamp, 
			"%Y-%m-%d %H:%M:%S", 
			szSyncUntilTimestamp, sizeof(szSyncUntilTimestamp)),
		formatDatetime(pServer->up_time, 
			"%Y-%m-%d %H:%M:%S", 
			szUpTime, sizeof(szUpTime)),
		pServer->total_mb,
		pServer->free_mb, 
		pServer->changelog_offset, 
		pServer->store_path_count, 
		pServer->storage_port, 
		pServer->storage_http_port, 
		pServer->subdir_count_per_path, 
		pServer->upload_priority,
		pServer->current_write_path,
		*(pServer->ref_count),
		pServer->chg_count,
#ifdef WITH_HTTPD
		pServer->http_check_last_errno,
		pServer->http_check_last_status,
		pServer->http_check_fail_count,
		pServer->http_check_error_info,
#endif
		pServer->stat.total_upload_count,
		pServer->stat.success_upload_count,
		pServer->stat.total_set_meta_count,
		pServer->stat.success_set_meta_count,
		pServer->stat.total_delete_count,
		pServer->stat.success_delete_count,
		pServer->stat.total_download_count,
		pServer->stat.success_download_count,
		pServer->stat.total_get_meta_count,
		pServer->stat.success_get_meta_count,
		pServer->stat.total_create_link_count,
		pServer->stat.success_create_link_count,
		pServer->stat.total_delete_link_count,
		pServer->stat.success_delete_link_count,
		formatDatetime(pServer->stat.last_source_update, 
			"%Y-%m-%d %H:%M:%S", 
			szSrcUpdTime, sizeof(szSrcUpdTime)), 
		formatDatetime(pServer->stat.last_sync_update,
			"%Y-%m-%d %H:%M:%S", 
			szSyncUpdTime, sizeof(szSyncUpdTime)), 
		formatDatetime(pServer->stat.last_synced_timestamp, 
			"%Y-%m-%d %H:%M:%S", 
			szSyncedTimestamp, sizeof(szSyncedTimestamp)),
		formatDatetime(pServer->stat.last_heart_beat_time,
			"%Y-%m-%d %H:%M:%S", 
			szLastHeartBeatTime, sizeof(szLastHeartBeatTime))
	);

	for (i=0; i<pServer->store_path_count; i++)
	{
		total_len += snprintf(buff + total_len, buffSize - total_len, 
			"disk %d: total_mb="INT64_PRINTF_FORMAT" MB, "
			"free_mb="INT64_PRINTF_FORMAT" MB\n",
			i+1, pServer->path_total_mbs[i],
			pServer->path_free_mbs[i]);
	}

	return total_len;
}

/*tracker的全局变量*/
static int fdfs_dump_global_vars(char *buff, const int buffSize)
{
	int total_len;

	total_len = snprintf(buff, buffSize,
		"g_fdfs_connect_timeout=%ds\n"
		"g_fdfs_network_timeout=%ds\n"
		"g_fdfs_base_path=%s\n"
		"g_fdfs_version=%d.%d\n"
		"g_continue_flag=%d\n"
		"g_schedule_flag=%d\n"
		"g_server_port=%d\n"
		"g_max_connections=%d\n"
		"g_tracker_thread_count=%d\n"
		"g_sync_log_buff_interval=%d\n"
		"g_check_active_interval=%d\n"
		"g_storage_stat_chg_count=%d\n"
		"g_storage_sync_time_chg_count=%d\n"
		"g_storage_reserved_mb=%d MB\n"
		"g_allow_ip_count=%d\n"
		"g_run_by_group=%s\n"
		"g_run_by_user=%s\n"
		"g_storage_ip_changed_auto_adjust=%d\n"
		"g_thread_kill_done=%d\n"
		"g_thread_stack_size=%d\n"
		"g_changelog_fsize="INT64_PRINTF_FORMAT"\n"
	#ifdef WITH_HTTPD
		"g_http_params.disabled=%d\n"
		"g_http_params.anti_steal_token=%d\n"
		"g_http_params.server_port=%d\n"
		"g_http_params.content_type_hash item count=%d\n"
		"g_http_params.anti_steal_secret_key length=%d\n"
		"g_http_params.token_check_fail_buff length=%d\n"
		"g_http_params.default_content_type=%s\n"
		"g_http_params.token_check_fail_content_type=%s\n"
		"g_http_params.token_ttl=%d\n"
		"g_http_check_interval=%d\n"
		"g_http_check_type=%d\n"
		"g_http_check_uri=%s\n"
		"g_http_servers_dirty=%d\n"
	#endif
	#if defined(DEBUG_FLAG) && defined(OS_LINUX)
		"g_exe_name=%s\n"
	#endif
		, g_fdfs_connect_timeout
		, g_fdfs_network_timeout
		, g_fdfs_base_path
		, g_fdfs_version.major, g_fdfs_version.minor
		, g_continue_flag
		, g_schedule_flag
		, g_server_port
		, g_max_connections
		, g_tracker_thread_count
		, g_sync_log_buff_interval
		, g_check_active_interval
		, g_storage_stat_chg_count
		, g_storage_sync_time_chg_count
		, g_storage_reserved_mb
		, g_allow_ip_count
		, g_run_by_group
		, g_run_by_user
		, g_storage_ip_changed_auto_adjust
		, g_thread_kill_done
		, g_thread_stack_size
		, g_changelog_fsize
	#ifdef WITH_HTTPD
		, g_http_params.disabled
		, g_http_params.anti_steal_token
		, g_http_params.server_port
		, hash_count(&(g_http_params.content_type_hash))
		, g_http_params.anti_steal_secret_key.length
		, g_http_params.token_check_fail_buff.length
		, g_http_params.default_content_type
		, g_http_params.token_check_fail_content_type
		, g_http_params.token_ttl
		, g_http_check_interval
		, g_http_check_type
		, g_http_check_uri
		, g_http_servers_dirty
	#endif
		
	#if defined(DEBUG_FLAG) && defined(OS_LINUX)
		, g_exe_name
	#endif
	);

	return total_len;
}

/*所有组的描述信息*/
static int fdfs_dump_groups_info(char *buff, const int buffSize)
{
	int total_len;

	total_len = snprintf(buff, buffSize, 
		"group count=%d\n"
		"group alloc_size=%d\n"
		"store_lookup=%d\n"
		"store_server=%d\n"
		"download_server=%d\n"
		"store_path=%d\n"
		"store_group=%s\n"
		"pStoreGroup=%s\n"
		"current_write_group=%d\n",
		g_groups.count, 
		g_groups.alloc_size, 
		g_groups.store_lookup, 
		g_groups.store_server, 
		g_groups.download_server, 
		g_groups.store_path, 
		g_groups.store_group, 
		g_groups.pStoreGroup != NULL ? 
			g_groups.pStoreGroup->group_name : "",
		g_groups.current_write_group
	);

	return total_len;
}

#define WRITE_TO_FILE(fd, buff, len) \
	if (write(fd, buff, len) != len) \
	{ \
		logError("file: "__FILE__", line: %d, " \
			"write to file %s fail, errno: %d, error info: %s", \
			__LINE__, filename, errno, strerror(errno)); \
		result = errno; \
		break; \
	}

/*打印全部信息*/
int fdfs_dump_tracker_global_vars_to_file(const char *filename)
{
	char buff[16 * 1024];
	char szCurrentTime[32];
	int len;
	int result;
	int fd;
	FDFSGroupInfo *pGroup;
	FDFSGroupInfo *pGroupEnd;
	FDFSGroupInfo **ppGroup;
	FDFSGroupInfo **ppGroupEnd;

	fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0)
	{
		logError("file: "__FILE__", line: %d, "
			"open file %s fail, errno: %d, error info: %s",
			__LINE__, filename, errno, strerror(errno));
		return errno;
	}

	do
	{
		result = 0;
		formatDatetime(time(NULL), "%Y-%m-%d %H:%M:%S", 
				szCurrentTime, sizeof(szCurrentTime));

		len = sprintf(buff, "\n====time: %s  DUMP START====\n", 
				szCurrentTime);
		WRITE_TO_FILE(fd, buff, len)

        /*首先是打印Tracker全局变量的信息*/
		len = fdfs_dump_global_vars(buff, sizeof(buff));
		WRITE_TO_FILE(fd, buff, len)

        /*然后打印描述所有组信息的信息*/
		len = fdfs_dump_groups_info(buff, sizeof(buff));
		WRITE_TO_FILE(fd, buff, len)

        /*其实这里感觉放到 fdfs_dump_groups_info更好点，*/
        /*因为都是描述所有组的信息（组名）的*/
		len = sprintf(buff, "\ngroup name list:\n");
		WRITE_TO_FILE(fd, buff, len)
		len = 0;
		pGroupEnd = g_groups.groups + g_groups.count;
		for (pGroup=g_groups.groups; pGroup<pGroupEnd; pGroup++)
		{
			len += sprintf(buff+len, "\t%s\n", pGroup->group_name);
		}
		len += sprintf(buff+len, "\n");
		WRITE_TO_FILE(fd, buff, len)

        /*打印各组的信息，在fdfs_dump_group_stat()中会继续打印组内各*/
        /*storage的信息*/
		ppGroupEnd = g_groups.sorted_groups + g_groups.count;
		for (ppGroup=g_groups.sorted_groups; ppGroup<ppGroupEnd; ppGroup++)
		{
			len = sprintf(buff, "\nGroup %d.\n", 
				(int)(ppGroup - g_groups.sorted_groups) + 1);
			WRITE_TO_FILE(fd, buff, len)

			len = fdfs_dump_group_stat(*ppGroup, buff, sizeof(buff));
			WRITE_TO_FILE(fd, buff, len)
		}

		len = sprintf(buff, "\n====time: %s  DUMP END====\n\n", 
				szCurrentTime);
		WRITE_TO_FILE(fd, buff, len)
	} while(0);

	close(fd);

	return result;
}


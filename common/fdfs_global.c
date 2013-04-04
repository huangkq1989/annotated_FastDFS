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
#include "logger.h"
#include "fdfs_global.h"

int g_fdfs_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
int g_fdfs_network_timeout = DEFAULT_NETWORK_TIMEOUT;
char g_fdfs_base_path[MAX_PATH_SIZE] = {'/', 't', 'm', 'p', '\0'};
Version g_fdfs_version = {1, 29};

/*
 * base_path下的目录为 "%02x/%02x/"
data filename format:
HH/HH/filename: HH for 2 uppercase hex chars
*/
int fdfs_check_data_filename(const char *filename, const int len)
{
	if (len < 6)
	{
		logError("file: "__FILE__", line: %d, " \
			"the length=%d of filename \"%s\" is too short", \
			__LINE__, len, filename);
		return EINVAL;
	}

	if (!IS_UPPER_HEX(*filename) || !IS_UPPER_HEX(*(filename+1)) || \
	    *(filename+2) != '/' || \
	    !IS_UPPER_HEX(*(filename+3)) || !IS_UPPER_HEX(*(filename+4)) || \
	    *(filename+5) != '/')
	{
		logError("file: "__FILE__", line: %d, " \
			"the format of filename \"%s\" is invalid", \
			__LINE__, filename);
		return EINVAL;
	}

	if (strchr(filename + 6, '/') != NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"the format of filename \"%s\" is invalid", \
			__LINE__, filename);
		return EINVAL;
	}

	return 0;
}

/*
 *dfs有从文件的概念，如：一张大图，通常会有缩略图，缩略图就是
 * 大图的从文件，这体现了fastdfs考虑了互联网应用特性
 * 从文件的名称为master无后缀部分加上prefix_name加上ext_name
 * 如果参数指定后缀，则取参数的，否则取master的
 */
int fdfs_gen_slave_filename(const char *master_filename, \
		const char *prefix_name, const char *ext_name, \
		char *filename, int *filename_len)
{
	char true_ext_name[FDFS_FILE_EXT_NAME_MAX_LEN + 2];
	char *pDot;
	int master_file_len;

	master_file_len = strlen(master_filename);
    /*
     *  28是魔数了，其实这种编程风格不好,fastdfs的文件名格式是：
     *  ip+时间戳+随机数+文件名，这几个会大于28
     */
	if (master_file_len < 28 + FDFS_FILE_EXT_NAME_MAX_LEN)
	{
		logError("file: "__FILE__", line: %d, " \
			"master filename \"%s\" is invalid", \
			__LINE__, master_filename);
		return EINVAL;
	}

	pDot = strchr(master_filename + (master_file_len - \
			(FDFS_FILE_EXT_NAME_MAX_LEN + 1)), '.');
    /*指定自定义后缀 */
	if (ext_name != NULL)
	{
		if (*ext_name == '\0')
		{
			*true_ext_name = '\0';
		}  
        /*.suffixname*/
		else if (*ext_name == '.')
		{
			snprintf(true_ext_name, sizeof(true_ext_name), \
				"%s", ext_name);
		}
        /*只有suffixname*/
		else
		{
			snprintf(true_ext_name, sizeof(true_ext_name), \
				".%s", ext_name);
		}
	}
    /*没有自定义后缀，从master取*/
	else
	{
        /*master file也没有后缀*/
		if (pDot == NULL)
		{
			*true_ext_name = '\0';
		}
        /*master file有后缀，从.开始复制*/
		else
		{
			strcpy(true_ext_name, pDot);
		}
	}

    /*如果没有后缀，且前缀为-m，那么不合法*/
	if (*true_ext_name == '\0' && strcmp(prefix_name, "-m") == 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"prefix_name \"%s\" is invalid", \
			__LINE__, prefix_name);
		return EINVAL;
	}

    /*master没有后缀*/
	if (pDot == NULL)
	{
		*filename_len = sprintf(filename, "%s%s%s", master_filename, \
			prefix_name, true_ext_name);
	}
	else
	{
		*filename_len = pDot - master_filename;
		memcpy(filename, master_filename, *filename_len);
		*filename_len += sprintf(filename + *filename_len, "%s%s", \
			prefix_name, true_ext_name);
	}

	return 0;
}


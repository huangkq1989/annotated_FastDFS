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

//storage_func.h

#ifndef _STORAGE_FUNC_H_
#define _STORAGE_FUNC_H_

#define STORAGE_DATA_DIR_FORMAT		"%02X"
#define STORAGE_META_FILE_EXT		"-m"
#define STORAGE_STORE_PATH_PREFIX_CHAR	'M'

#ifdef __cplusplus
extern "C" {
#endif

typedef char * (*get_filename_func)(const void *pArg, \
			char *full_filename);

int storage_write_to_fd(int fd, get_filename_func filename_func, \
		const void *pArg, const char *buff, const int len);

/*读取配置，检查ip变化，获取changelog，读取state文件*/
int storage_func_init(const char *filename, \
		char *bind_addr, const int addr_size);
int storage_func_destroy();

int storage_write_to_stat_file();

int storage_write_to_sync_ini_file();

int storage_split_filename(const char *logic_filename, \
		int *filename_len, char *true_filename, char **ppStorePath);
int storage_split_filename_ex(const char *logic_filename, \
		int *filename_len, char *true_filename, int *store_path_index);

/*
int write_serialized(int fd, const char *buff, size_t count, const bool bSync);
int fsync_serialized(int fd);
int recv_file_serialized(int sock, const char *filename, \
		const int64_t file_bytes);
*/

#ifdef __cplusplus
}
#endif

#endif

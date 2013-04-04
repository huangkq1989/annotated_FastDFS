/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * tracker和storage公用的http数据和函数
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

#ifndef _FDFS_HTTP_SHARED_H
#define _FDFS_HTTP_SHARED_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "ini_file_reader.h"
#include "hash.h"

/*http参数:相关参数说明见http.conf*/
typedef struct 
{
    bool disabled;
    bool anti_steal_token;     //是否防盗链
    int server_port;

    /* key is file ext name, value is content type */
    HashArray content_type_hash;   

    BufferInfo anti_steal_secret_key;
    BufferInfo token_check_fail_buff;
    /*若根据mime_file解析不到，则使用下面的默认的content_type*/
    char default_content_type[64];
    char token_check_fail_content_type[64];
    int token_ttl; // time to live of token
} FDFSHTTPParams;

#ifdef __cplusplus
extern "C" {
#endif

    /**
      load HTTP params from conf file
params:
pIniContext: the ini file items, return by iniLoadItems
conf_filename: config filename
pHTTPParams: the HTTP params
return: 0 for success, != 0 fail
     **/
    int fdfs_http_params_load(IniContext *pIniContext, \
            const char *conf_filename, FDFSHTTPParams *pHTTPParams);

    void fdfs_http_params_destroy(FDFSHTTPParams *pParams);

    /**
      generate anti-steal token
params:
secret_key: secret key buffer
file_id: FastDFS file id
timestamp: current timestamp, unix timestamp (seconds)
token: return token buffer
return: 0 for success, != 0 fail
     **/
    int fdfs_http_gen_token(const BufferInfo *secret_key, const char *file_id, \
            const int timestamp, char *token);

    /**
      check anti-steal token
params:
secret_key: secret key buffer
file_id: FastDFS file id
timestamp: the timestamp to generate the token, unix timestamp (seconds)
token: token buffer
ttl: token ttl, delta seconds
return: 0 for passed, != 0 fail
     **/
    int fdfs_http_check_token(const BufferInfo *secret_key, const char *file_id, \
            const int timestamp, const char *token, const int ttl);

    /**
      get parameter value
params:
param_name: the parameter name to get
params: parameter array
param_count: param count
return: param value pointer, return NULL if not exist
     **/
    char *fdfs_http_get_parameter(const char *param_name, KeyValuePair *params, \
            const int param_count);

    /**
      get content type by file extension name
params:
pHTTPParams: the HTTP params
filename: the filename
content_type: return content type
content_type_size: content type buffer size
return: 0 for success, != 0 fail
     **/
    int fdfs_http_get_content_type_by_extname(FDFSHTTPParams *pParams, \
            const char *filename, char *content_type, const int content_type_size);

#ifdef __cplusplus
}
#endif

#endif


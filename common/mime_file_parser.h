/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 从mime.types文件加载 mime types到Hash表中，以便
 * 根据文件名后缀确定http的content_type的值
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

#ifndef _MINE_FILE_PARSER_H
#define _MINE_FILE_PARSER_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "hash.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
      load mime types from file
params:
pHash: hash array to store the mime types, 
key is the file extension name, eg. jpg
value is the content type, eg. image/jpeg
the hash array will be initialized in this function,
the hash array should be destroyed when used done
mime_filename: the mime filename, 
file format is same as apache's file: mime.types
return: 0 for success, !=0 for fail
     **/
    int load_mime_types_from_file(HashArray *pHash, const char *mime_filename);

#ifdef __cplusplus
}
#endif

#endif


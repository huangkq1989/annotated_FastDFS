/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 本文件处理加载配置文件，加载后把参数覆盖Tracker的
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

//tracker_func.h

#ifndef _TRACKER_FUNC_H_
#define _TRACKER_FUNC_H_

#ifdef __cplusplus
extern "C" {
#endif

    int tracker_load_from_conf_file(const char *filename, \
            char *bind_addr, const int addr_size);

#ifdef __cplusplus
}
#endif

#endif

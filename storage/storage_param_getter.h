/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 从Tracker获取参数，本版本只有ip改变时是否自动调整
 * 这一参数从Tracker获取
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//storage_param_getter.h

#ifndef _STORAGE_PARAM_GETTER_H_
#define _STORAGE_PARAM_GETTER_H_

#include "tracker_types.h"
#include "tracker_client_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

    int storage_get_params_from_tracker();

#ifdef __cplusplus
}
#endif

#endif


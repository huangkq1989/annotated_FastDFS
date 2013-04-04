/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 读取配置文件的处理函数
 * 配置文件的格式： 
 * item_key1=item_value1 #没section的名字的为global
 * [section1]
 * item_key1=item_value1
 * item_key2=item_value2
 * item_key3=item_value3
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//ini_file_reader.h
#ifndef INI_FILE_READER_H
#define INI_FILE_READER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_define.h"
#include "hash.h"

#define INI_ITEM_NAME_LEN		64
#define INI_ITEM_VALUE_LEN		128

typedef struct
{
    char name[INI_ITEM_NAME_LEN + 1];
    char value[INI_ITEM_VALUE_LEN + 1];
} IniItem;

typedef struct
{
    IniItem *items;
    int count;  //item count
    int alloc_count;
} IniSection;

typedef struct
{
    IniSection global;  //全局section的配置数据
    HashArray sections; //非全局section的配置数据，section名为HashArray的key,
                        //section的配置数据为HashArray的value.
    IniSection *current_section; //for load from ini file
} IniContext;

#ifdef __cplusplus
extern "C" {
#endif

    /** load ini items from file
     *  parameters:
     *           szFilename: the filename, can be an URL
     *           pContext: the ini context
     *  return: error no, 0 for success, != 0 fail
     */
    int iniLoadFromFile(const char *szFilename, IniContext *pContext);

    /** load ini items from string buffer
     *  parameters:
     *           content: the string buffer to parse
     *           pContext: the ini context
     *  return: error no, 0 for success, != 0 fail
     */
    int iniLoadFromBuffer(char *content, IniContext *pContext);

    /** free ini context
     *  parameters:
     *           pContext: the ini context
     *  return: none
     */
    void iniFreeContext(IniContext *pContext);

    /** get item string value
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *  return: item value, return NULL when the item not exist
     */
    char *iniGetStrValue(const char *szSectionName, const char *szItemName, \
            IniContext *pContext);

    /** get item string value
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *           szValues:   string array to store the values
     *           max_values: max string array elements
     *  return: item value count
     */
    int iniGetValues(const char *szSectionName, const char *szItemName, \
            IniContext *pContext, char **szValues, const int max_values);

    /** get item int value (32 bits)
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *           nDefaultValue: the default value
     *  return: item value, return nDefaultValue when the item not exist
     */
    int iniGetIntValue(const char *szSectionName, const char *szItemName, \
            IniContext *pContext, const int nDefaultValue);

    /** get item string value array
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *           nTargetCount: store the item value count
     *  return: item value array, return NULL when the item not exist
     */
    IniItem *iniGetValuesEx(const char *szSectionName, const char *szItemName, \
            IniContext *pContext, int *nTargetCount);

    /** get item int64 value (64 bits)
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *           nDefaultValue: the default value
     *  return: int64 value, return nDefaultValue when the item not exist
     */
    int64_t iniGetInt64Value(const char *szSectionName, const char *szItemName, \
            IniContext *pContext, const int64_t nDefaultValue);

    /** get item boolean value
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *           bDefaultValue: the default value
     *  return: item boolean value, return bDefaultValue when the item not exist
     */
    bool iniGetBoolValue(const char *szSectionName, const char *szItemName, \
            IniContext *pContext, const bool bDefaultValue);

    /** get item double value
     *  parameters:
     *           szSectionName: the section name, NULL or empty string for 
     *                          global section
     *           szItemName: the item name
     *           pContext:   the ini context
     *           dbDefaultValue: the default value
     *  return: item value, return dbDefaultValue when the item not exist
     */
    double iniGetDoubleValue(const char *szSectionName, const char *szItemName, \
            IniContext *pContext, const double dbDefaultValue);

    /** print all items
     *  parameters:
     *           pContext:   the ini context
     *  return: none
     */
    void iniPrintItems(IniContext *pContext);

#ifdef __cplusplus
}
#endif

#endif


/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 实现base64,用于对文件名编码
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//base64.h

#ifndef _BASE64_H
#define _BASE64_H

#include "common_define.h"

/*用于base64编码*/
struct base64_context
{
    /*每行的换行符，最大可以为16*/
    char line_separator[16];
    /*换行符的长度*/
    int line_sep_len;

    /**
     * max chars per line, excluding line_separator.  A multiple of 4.
     */
    /*每行的长度*/
    int line_length;

    /**
     * letter of the alphabet used to encode binary values 0..63
     * 为什么是64?
     * base64编码中，每6位前面补两位0构成一个字节然后转为一个字符，
     * 共有2^6=64个不同的字符
     */
    unsigned char valueToChar[64];

    /**
     * binary value encoded by a given letter of the alphabet 0..63
     * 由charToValue['A']可直接得到A的对应的字节内容
     * 为什么是256？
     * 由字符转回字节，A-Z,=这些为ASCII，最大为2^8=256.	
     */
    int charToValue[256];
    /* 不够4个，则用pad_ch填充*/
    int pad_ch;
};

#ifdef __cplusplus
extern "C" {
#endif

    /** use stardand base64 charset
    */
#define base64_init(context, nLineLength) \
    base64_init_ex(context, nLineLength, '+', '/', '=')


    /** stardand base64 encode
    */
#define base64_encode(context, src, nSrcLen, dest, dest_len) \
    base64_encode_ex(context, src, nSrcLen, dest, dest_len, true)

    /**
     * 初始化结构体，10个数字 + 26个小写字母 + 26个大写字母 + ‘+’ + ‘/’ = 64,
     * =号是用来填补的
     *  base64 init function, before use base64 function, you should call 
     *  this function
     *  parameters:
     *           context:     the base64 context
     *           nLineLength: length of a line, 0 for never add line seperator
     *           chPlus:      specify a char for base64 char plus (+)
     *           chSplash:    specify a char for base64 char splash (/)
     *           chPad:       specify a char for base64 padding char =
     *  return: none
     */
    void base64_init_ex(struct base64_context *context, const int nLineLength, \
            const unsigned char chPlus, const unsigned char chSplash, \
            const unsigned char chPad);

    /** 
     *  计算编码后的长度是多少
     *
     *  calculate the encoded length of the source buffer
     *  parameters:
     *           context:     the base64 context
     *           nSrcLen:     the source buffer length
     *  return: the encoded length of the source buffer
     */
    int base64_get_encode_length(struct base64_context *context, const int nSrcLen);

    /** 
     *  开始编码
     *
     *  base64 encode buffer
     *  parameters:
     *           context:   the base64 context
     *           src:       the source buffer
     *           nSrcLen:   the source buffer length
     *           dest:      the dest buffer
     *           dest_len:  return dest buffer length
     *           bPad:      if padding
     *  return: the encoded buffer
     */
    char *base64_encode_ex(struct base64_context *context, char *src, \
            const int nSrcLen, char *dest, int *dest_len, const bool bPad);

    /** 
     *  进行解码
     *
     *  base64 decode buffer, work only with padding source string
     *  parameters:
     *           context:   the base64 context
     *           src:       the source buffer with padding
     *           nSrcLen:   the source buffer length
     *           dest:      the dest buffer
     *           dest_len:  return dest buffer length
     *  return: the decoded buffer
     */
    char *base64_decode(struct base64_context *context, char *src, \
            const int nSrcLen, char *dest, int *dest_len);

    /** 
     *  进行解码，处理没有pad的情况
     *
     *  base64 decode buffer, can work with no padding source string
     *  parameters:
     *           context:   the base64 context
     *           src:       the source buffer with padding or no padding
     *           nSrcLen:   the source buffer length
     *           dest:      the dest buffer
     *           dest_len:  return dest buffer length
     *  return: the decoded buffer
     */
    char *base64_decode_auto(struct base64_context *context, char *src, \
            const int nSrcLen, char *dest, int *dest_len);

    /** 
     *  设置换行的字符串
     *
     *  set line separator string, such as \n or \r\n
     *  parameters:
     *           context:   the base64 context
     *           pLineSeparator: the line separator string
     *  return: none
     */
    void base64_set_line_separator(struct base64_context *context, \
            const char *pLineSeparator);

    /** 
     *  设置换行字符串的长度
     *
     *  set line length, 0 for never add line separators
     *  parameters:
     *           context:   the base64 context
     *           length:    the line length
     *  return: none
     */
    void base64_set_line_length(struct base64_context *context, const int length);

#ifdef __cplusplus
}
#endif

#endif


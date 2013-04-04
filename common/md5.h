/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 计算文件，字符串或缓冲的MD5值,在本系统中主要
 * 用于查重,则通过计算文件的MD5，放到FDHT这个分
 * 布式HASH系统中，通过检查文件的MD5是否已在系统
 * 中来判断文件是否已经存在
 *
 **************************************************/

#ifndef MCL_MD5_H
#define MCL_MD5_H
#include <stdio.h>

typedef unsigned char *POINTER;
typedef unsigned short int UINT2;
typedef unsigned int UINT4;

typedef struct {
  UINT4 state[4];		/* state (ABCD) */
  UINT4 count[2];		/* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];	/* input buffer */
} MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

/** md5 for string
 *  parameters:
 *           string: the string to md5
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int MD5String(char *string, unsigned char digest[16]);

/** md5 for file
 *  parameters:
 *           filename: the filename whose content to md5
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int MD5File(char *filename, unsigned char digest[16]);

/** md5 for buffer
 *  parameters:
 *           buffer: the buffer to md5
 *           len: the buffer length
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int MD5Buffer(char *buffer, unsigned int len, unsigned char digest[16]);

#ifdef __cplusplus
}
#endif

#endif


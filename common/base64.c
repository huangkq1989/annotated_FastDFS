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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "base64.h"

/**
 * Marker value for chars we just ignore, e.g. \n \r high ascii
 */
#define IGNORE  -1

/**
 * Marker for = trailing pad
 */
#define PAD   -2

/**
 *
 * determines how long the lines are that are generated by encode.
 * Ignored by decode.
 * @param length 0 means no newlines inserted. Must be a multiple of 4.
 */
void base64_set_line_length(struct base64_context *context, const int length)
{
    context->line_length = (length / 4) * 4;
}

/**
 * How lines are separated.
 * Ignored by decode.
 * @param context->line_separator may be "" but not null.
 * Usually contains only a combination of chars \n and \r.
 * Could be any chars not in set A-Z a-z 0-9 + /.
 */
void base64_set_line_separator(struct base64_context *context, \
        const char *pLineSeparator)
{
    context->line_sep_len = snprintf(context->line_separator, \
            sizeof(context->line_separator), "%s", pLineSeparator);
}

void base64_init_ex(struct base64_context *context, const int nLineLength, \
        const unsigned char chPlus, const unsigned char chSplash, \
        const unsigned char chPad)
{
    int i;

    /*对结构体清空*/
    memset(context, 0, sizeof(struct base64_context));

    context->line_length = nLineLength;
    context->line_separator[0] = '\n';
    context->line_separator[1] = '\0';
    context->line_sep_len = 1;

    /*设置由字节到字符的映射*/
    // build translate valueToChar table only once.
    // 0..25 -> 'A'..'Z'
    for (i=0; i<=25; i++)
    {
        context->valueToChar[i] = (char)('A'+i);
    }
    // 26..51 -> 'a'..'z'
    for (i=0; i<=25; i++ )
    {
        context->valueToChar[i+26] = (char)('a'+i);
    }
    // 52..61 -> '0'..'9'
    for (i=0; i<=9; i++ )
    {
        context->valueToChar[i+52] = (char)('0'+i);
    }
    context->valueToChar[62] = chPlus;
    context->valueToChar[63] = chSplash;

    /*设置由字符到字节的映射*/
    memset(context->charToValue, IGNORE, sizeof(context->charToValue));
    for (i=0; i<64; i++ )
    {
        context->charToValue[context->valueToChar[i]] = i;
    }

    context->pad_ch = chPad;
    context->charToValue[chPad] = PAD;
}

int base64_get_encode_length(struct base64_context *context, const int nSrcLen)
{
    // Each group or partial group of 3 bytes becomes four chars
    // covered quotient
    int outputLength;

    /*每6位补两位变一个字节，则每3个字节变成4个字节，
     * 加2是处理除3有余数的情况*/
    outputLength = ((nSrcLen + 2) / 3) * 4;

    // account for trailing newlines, on all but the very last line
    /*等于0就不用换行了*/
    if (context->line_length != 0)
    {
        /*计算一共有多少行，加上（context->line_length-1）是为了处理
         * 除context->line_length有余数的情况, 最后一行不加换行符，所以减1*/
        int lines =  (outputLength + context->line_length - 1) / 
            context->line_length - 1;
        if ( lines > 0 )
        {
            /*加上每行的换行符长度*/
            outputLength += lines  * context->line_sep_len;
        }
    }

    return outputLength;
}

/**
 * Encode an arbitrary array of bytes as base64 printable ASCII.
 * It will be broken into lines of 72 chars each.  The last line is not
 * terminated with a line separator.
 * The output will always have an even multiple of data characters,
 * exclusive of \n.  It is padded out with =.
 */
char *base64_encode_ex(struct base64_context *context, char *src, \
        const int nSrcLen, char *dest, int *dest_len, const bool bPad)
{
    int linePos;
    int leftover;
    int combined;
    char *pDest;
    int c0, c1, c2, c3;
    unsigned char *pRaw;
    unsigned char *pEnd;
    char *ppSrcs[2];
    int lens[2];
    char szPad[3];
    int k;
    int loop;

    if (nSrcLen <= 0)
    {
        *dest = '\0';
        *dest_len = 0;
        return dest;
    }

    linePos = 0;
    lens[0] = (nSrcLen / 3) * 3;
    lens[1] = 3;
    leftover = nSrcLen - lens[0];
    ppSrcs[0] = src;
    ppSrcs[1] = szPad;

    szPad[0] = szPad[1] = szPad[2] = '\0';
    switch (leftover)
    {
        case 0:
        default:
            loop = 1;
            break;
        case 1:
            loop = 2;
            szPad[0] = src[nSrcLen-1];
            break;
        case 2:
            loop = 2;
            szPad[0] = src[nSrcLen-2];
            szPad[1] = src[nSrcLen-1];
            break;
    }

    pDest = dest;
    for (k=0; k<loop; k++)
    {
        pEnd = (unsigned char *)ppSrcs[k] + lens[k];
        for (pRaw=(unsigned char *)ppSrcs[k]; pRaw<pEnd; pRaw+=3)
        {
            // Start a new line if next 4 chars won't fit on the current line
            // We can't encapsulete the following code since the variable need to
            // be local to this incarnation of encode.
            linePos += 4;
            if (linePos > context->line_length)
            {
                if (context->line_length != 0)
                {
                    memcpy(pDest, context->line_separator, context->line_sep_len);
                    pDest += context->line_sep_len;
                }
                linePos = 4;
            }

            // get next three bytes in unsigned form lined up,
            // in big-endian order
            combined = ((*pRaw) << 16) | ((*(pRaw+1)) << 8) | (*(pRaw+2));

            // break those 24 bits into a 4 groups of 6 bits,
            // working LSB to MSB.
            c3 = combined & 0x3f;
            combined >>= 6;
            c2 = combined & 0x3f;
            combined >>= 6;
            c1 = combined & 0x3f;
            combined >>= 6;
            c0 = combined & 0x3f;

            // Translate into the equivalent alpha character
            // emitting them in big-endian order.
            *pDest++ = context->valueToChar[c0];
            *pDest++ = context->valueToChar[c1];
            *pDest++ = context->valueToChar[c2];
            *pDest++ = context->valueToChar[c3];
        }
    }

    *pDest = '\0';
    *dest_len = pDest - dest;

    // deal with leftover bytes
    switch (leftover)
    {
        case 0:
        default:
            // nothing to do
            break;
        case 1:
            // One leftover byte generates xx==
            if (bPad)
            {
                *(pDest-1) = context->pad_ch;
                *(pDest-2) = context->pad_ch;
            }
            else
            {
                *(pDest-2) = '\0';
                *dest_len -= 2;
            }
            break;
        case 2:
            // Two leftover bytes generates xxx=
            if (bPad)
            {
                *(pDest-1) = context->pad_ch;
            }
            else
            {
                *(pDest-1) = '\0';
                *dest_len -= 1;
            }
            break;
    } // end switch;

    return dest;
}

char *base64_decode_auto(struct base64_context *context, char *src, \
        const int nSrcLen, char *dest, int *dest_len)
{
    int nRemain;
    int nPadLen;
    int nNewLen;
    char tmpBuff[256];
    char *pBuff;

    nRemain = nSrcLen % 4;
    if (nRemain == 0)
    {
        return base64_decode(context, src, nSrcLen, dest, dest_len);
    }

    nPadLen = 4 - nRemain;
    nNewLen = nSrcLen + nPadLen;
    if (nNewLen <= sizeof(tmpBuff))
    {
        pBuff = tmpBuff;
    }
    else
    {
        pBuff = (char *)malloc(nNewLen);
        if (pBuff == NULL)
        {
            fprintf(stderr, "Can't malloc %d bytes\n", \
                    nSrcLen + nPadLen + 1);
            *dest_len = 0;
            *dest = '\0';
            return dest;
        }
    }

    memcpy(pBuff, src, nSrcLen);
    memset(pBuff + nSrcLen, context->pad_ch, nPadLen);

    base64_decode(context, pBuff, nNewLen, dest, dest_len);

    if (pBuff != tmpBuff)
    {
        free(pBuff);
    }

    return dest;
}

/**
 * decode a well-formed complete base64 string back into an array of bytes.
 * It must have an even multiple of 4 data characters (not counting \n),
 * padded out with = as needed.
 */
char *base64_decode(struct base64_context *context, char *src, \
        const int nSrcLen, char *dest, int *dest_len)
{
    // tracks where we are in a cycle of 4 input chars.
    int cycle;

    // where we combine 4 groups of 6 bits and take apart as 3 groups of 8.
    int combined;

    // will be an even multiple of 4 chars, plus some embedded \n
    int dummies;
    int value;
    unsigned char *pSrc;
    unsigned char *pSrcEnd;
    char *pDest;

    cycle = 0;
    combined = 0;
    dummies = 0;
    pDest = dest;
    pSrcEnd = (unsigned char *)src + nSrcLen;
    for (pSrc=(unsigned char *)src; pSrc<pSrcEnd; pSrc++)
    {
        value = context->charToValue[*pSrc];
        switch (value)
        {
            case IGNORE:
                // e.g. \n, just ignore it.
                break;
            case PAD:
                value = 0;
                dummies++;
                // fallthrough
            default:
                /* regular value character */
                switch (cycle)
                {
                    case 0:
                        combined = value;
                        cycle = 1;
                        break;
                    case 1:
                        combined <<= 6;
                        combined |= value;
                        cycle = 2;
                        break;
                    case 2:
                        combined <<= 6;
                        combined |= value;
                        cycle = 3;
                        break;
                    case 3:
                        combined <<= 6;
                        combined |= value;
                        // we have just completed a cycle of 4 chars.
                        // the four 6-bit values are in combined in big-endian order
                        // peel them off 8 bits at a time working lsb to msb
                        // to get our original 3 8-bit bytes back

                        *pDest++ = (char)(combined >> 16);
                        *pDest++ = (char)((combined & 0x0000FF00) >> 8);
                        *pDest++ = (char)(combined & 0x000000FF);

                        cycle = 0;
                        break;
                }
                break;
        }
    } // end for

    if (cycle != 0)
    {
        *dest = '\0';
        *dest_len = 0;
        fprintf(stderr, "Input to decode not an even multiple of " \
                "4 characters; pad with %c\n", context->pad_ch);
        return dest;
    }

    *dest_len = (pDest - dest) - dummies;
    *(dest + (*dest_len)) = '\0';

    return dest;
}


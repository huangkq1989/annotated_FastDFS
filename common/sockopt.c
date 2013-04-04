/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 处理socket操作
 * 比较特别的是编写了超时处理的读写和连接操作,
 * 另外封装了sendfile函数，避免发送文件时的在
 * 用户态和内核态的多次拷贝
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//socketopt.c
#include "common_define.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/poll.h>
#include <sys/select.h>

#ifdef OS_SUNOS
#include <sys/sockio.h>
#endif

#ifdef USE_SENDFILE

#ifdef OS_LINUX
#include <sys/sendfile.h>
#else
#ifdef OS_FREEBSD
#include <sys/uio.h>
#endif
#endif

#endif

#include "logger.h"
#include "hash.h"
#include "sockopt.h"

//#define USE_SELECT
#define USE_POLL

#ifdef OS_LINUX
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE	 4	/* Start keeplives after this period */
#endif

#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL	 5	/* Interval between keepalives */
#endif

#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 	6	/* Number of keepalives before death */
#endif
#endif

int tcpgets(int sock, char* s, const int size, const int timeout)
{
    int result;
    char t;
    int i=1;

    if (s == NULL || size <= 0)
    {
        return EINVAL;
    }

    while (i < size)
    {
        result = tcprecvdata(sock, &t, 1, timeout);
        if (result != 0)
        {
            *s = 0;
            return result;
        }

        if (t == '\r')
        {
            continue;
        }

        /*接收到换行符，返回*/
        if (t == '\n')
        {
            *s = t;
            s++;
            *s = 0;
            return 0;
        }

        *s = t;
        s++,i++;
    }

    /*最后一个字符为结束符*/
    *s = 0;
    return 0;
}

int tcprecvdata_ex(int sock, void *data, const int size, \
        const int timeout, int *count)
{
    int left_bytes;
    int read_bytes;
    int res;
    int ret_code;
    unsigned char* p;
#ifdef USE_SELECT
    fd_set read_set;
    struct timeval t;
#else
    struct pollfd pollfds;
#endif

#ifdef USE_SELECT
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);
#else
    pollfds.fd = sock;
    pollfds.events = POLLIN;
#endif

    read_bytes = 0;
    ret_code = 0;
    p = (unsigned char*)data;
    left_bytes = size;
    while (left_bytes > 0)
    {

        /*超时检查*/
#ifdef USE_SELECT
        if (timeout <= 0)
        {
            res = select(sock+1, &read_set, NULL, NULL, NULL);
        }
        else
        {
            t.tv_usec = 0;
            t.tv_sec = timeout;
            res = select(sock+1, &read_set, NULL, NULL, &t);
        }
#else
        res = poll(&pollfds, 1, 1000 * timeout);
        if (pollfds.revents & POLLHUP)
        {
            ret_code = ENOTCONN;
            break;
        }
#endif

        /*如果有事件发生，那么res将会大于0*/
        if (res < 0) //出错
        {
            ret_code = errno != 0 ? errno : EINTR;
            break;
        }
        else if (res == 0) // 超时
        {
            ret_code = ETIMEDOUT;
            break;
        }

        /*
         *recv在最后一个参数为0时，缓冲中有多少可读
         *就读多少，所以需要循环，才能读到指定的数
         *目的数据
         */
        read_bytes = recv(sock, p, left_bytes, 0);
        if (read_bytes < 0)
        {
            ret_code = errno != 0 ? errno : EINTR;
            break;
        }
        /*进入循环说明还有数据，但读到的数为，说明连接断了*/
        if (read_bytes == 0)
        {
            ret_code = ENOTCONN;
            break;
        }

        left_bytes -= read_bytes;
        p += read_bytes;
    }

    if (count != NULL)
    {
        *count = size - left_bytes;
    }

    return ret_code;
}

/*基本同上一函数，只是把接收改为发送*/
int tcpsenddata(int sock, void* data, const int size, const int timeout)
{
    int left_bytes;
    int write_bytes;
    int result;
    unsigned char* p;
#ifdef USE_SELECT
    fd_set write_set;
    struct timeval t;
#else
    struct pollfd pollfds;
#endif

#ifdef USE_SELECT
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);
#else
    pollfds.fd = sock;
    pollfds.events = POLLOUT;
#endif

    p = (unsigned char*)data;
    left_bytes = size;
    while (left_bytes > 0)
    {
#ifdef USE_SELECT
        if (timeout <= 0)
        {
            result = select(sock+1, NULL, &write_set, NULL, NULL);
        }
        else
        {
            t.tv_usec = 0;
            t.tv_sec = timeout;
            result = select(sock+1, NULL, &write_set, NULL, &t);
        }
#else
        result = poll(&pollfds, 1, 1000 * timeout);
        if (pollfds.revents & POLLHUP)
        {
            return ENOTCONN;
            break;
        }
#endif

        if (result < 0)
        {
            return errno != 0 ? errno : EINTR;
        }
        else if (result == 0)
        {
            return ETIMEDOUT;
        }

        write_bytes = send(sock, p, left_bytes, 0);
        if (write_bytes < 0)
        {
            return errno != 0 ? errno : EINTR;
        }
        /*
         *注意，这里没有判断wirte_bytes==0的情况，
         *因为就算连接断了，还是会返回可写到缓存
         *区的字节数的
         */

        left_bytes -= write_bytes;
        p += write_bytes;
    }

    return 0;
}

/*
 *与tcprecvdata_ex的区别是tcprecvdata_nb_ex适合于在设置了
 *非阻塞的情况调用，tcprecvdata_ex适合于在socket为阻塞调用
 *tcprecvdata_nb_ex的效率要比tcprecvdata_ex的高，因为避免了
 *每次循环都调用select/poll
 */
int tcprecvdata_nb_ex(int sock, void *data, const int size, \
        const int timeout, int *count)
{
    int left_bytes;
    int read_bytes;
    int res;
    int ret_code;
    unsigned char* p;
#ifdef USE_SELECT
    fd_set read_set;
    struct timeval t;
#else
    struct pollfd pollfds;
#endif

#ifdef USE_SELECT
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);
#else
    pollfds.fd = sock;
    pollfds.events = POLLIN;
#endif

    read_bytes = 0;
    ret_code = 0;
    p = (unsigned char*)data;
    left_bytes = size;
    while (left_bytes > 0)
    {
        read_bytes = recv(sock, p, left_bytes, 0);
        if (read_bytes > 0)
        {
            left_bytes -= read_bytes;
            p += read_bytes;
            continue;
        }

        if (read_bytes < 0)
        {
            /* 
             * 我认为:
             * 检查错误码是否为EINTR，
             * 如果是EINTR直接继续循环, 则中断重启
             *
             * 因为EINTR是在没有数据可用前，接收到SIGINTR造成的
             * EINTR  The receive was interrupted by delivery
             * of a signal before any data were available
             * 对EINTR的处理能减少读到非期望值数目的数据的次数
             *
             * 具体做法如下：
             *if (errno == EINTR) {
             *    continue;
             *}
             */
            /*
             *返回值为负数，且错误码为EAGAIN/EWOULDBLOCK时
             *进入等待超时检查
             */
            if (!(errno == EAGAIN || errno == EWOULDBLOCK))
            {
                ret_code = errno != 0 ? errno : EINTR;
                break;
            }
        }
        else
        {
            ret_code = ENOTCONN;
            break;
        }

#ifdef USE_SELECT
        if (timeout <= 0)
        {
            res = select(sock+1, &read_set, NULL, NULL, NULL);
        }
        else
        {
            t.tv_usec = 0;
            t.tv_sec = timeout;
            res = select(sock+1, &read_set, NULL, NULL, &t);
        }
#else
        res = poll(&pollfds, 1, 1000 * timeout);
        if (pollfds.revents & POLLHUP)
        {
            ret_code = ENOTCONN;
            break;
        }
#endif

        if (res < 0)
        {
            ret_code = errno != 0 ? errno : EINTR;
            break;
        }
        else if (res == 0)
        {
            ret_code = ETIMEDOUT;
            break;
        }
    }

    if (count != NULL)
    {
        *count = size - left_bytes;
    }

    return ret_code;
}

/*基本和tcprecvdata_nb_ex差不多，只是把读改为写*/
int tcpsenddata_nb(int sock, void* data, const int size, const int timeout)
{
    int left_bytes;
    int write_bytes;
    int result;
    unsigned char* p;
#ifdef USE_SELECT
    fd_set write_set;
    struct timeval t;
#else
    struct pollfd pollfds;
#endif

#ifdef USE_SELECT
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);
#else
    pollfds.fd = sock;
    pollfds.events = POLLOUT;
#endif

    p = (unsigned char*)data;
    left_bytes = size;
    while (left_bytes > 0)
    {
        write_bytes = send(sock, p, left_bytes, 0);
        if (write_bytes < 0)
        {
            if (!(errno == EAGAIN || errno == EWOULDBLOCK))
            {
                return errno != 0 ? errno : EINTR;
            }
        }
        /**
         * 在网络断开的情况下，send返回值还是要发送的数据大小
         * 所以没有处理返回值为0的情况
         **/
        else
        {
            left_bytes -= write_bytes;
            p += write_bytes;
            continue;
        }

#ifdef USE_SELECT
        if (timeout <= 0)
        {
            result = select(sock+1, NULL, &write_set, NULL, NULL);
        }
        else
        {
            t.tv_usec = 0;
            t.tv_sec = timeout;
            result = select(sock+1, NULL, &write_set, NULL, &t);
        }
#else
        result = poll(&pollfds, 1, 1000 * timeout);
        if (pollfds.revents & POLLHUP)
        {
            return ENOTCONN;
        }
#endif

        if (result < 0)
        {
            return errno != 0 ? errno : EINTR;
        }
        else if (result == 0)
        {
            return ETIMEDOUT;
        }
    }

    return 0;
}

/*不会超时*/
int connectserverbyip(int sock, const char *server_ip, const short server_port)
{
    int result;
    struct sockaddr_in addr;

    addr.sin_family = PF_INET;
    addr.sin_port = htons(server_port);
    result = inet_aton(server_ip, &addr.sin_addr);
    if (result == 0 )
    {
        return EINVAL;
    }

    result = connect(sock, (const struct sockaddr*)&addr, sizeof(addr));
    if (result < 0)
    {
        return errno != 0 ? errno : EINTR;
    }

    return 0;
}

/*会超时*/
int connectserverbyip_nb(int sock, const char *server_ip, \
        const short server_port, const int timeout)
{
    int result;
    int flags;
    int needRestore;
    socklen_t len;

#ifdef USE_SELECT
    fd_set rset;
    fd_set wset;
    struct timeval tval;
#else
    struct pollfd pollfds;
#endif

    struct sockaddr_in addr;

    addr.sin_family = PF_INET;
    addr.sin_port = htons(server_port);
    result = inet_aton(server_ip, &addr.sin_addr);
    if (result == 0 )
    {
        return EINVAL;
    }


    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    if ((flags & O_NONBLOCK) == 0)
    {
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            return errno != 0 ? errno : EACCES;
        }

        needRestore = 1;
    }
    else
    {
        needRestore = 0;
    }

    do
    {
        if (connect(sock, (const struct sockaddr*)&addr, \
                    sizeof(addr)) < 0)
        {
            result = errno != 0 ? errno : EINPROGRESS;
            if (result != EINPROGRESS)
            {
                break;
            }
        }
        else
        {
            result = 0;
            break;
        }


        /*正在处理,然后开始等*/
#ifdef USE_SELECT
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(sock, &rset);
        FD_SET(sock, &wset);
        tval.tv_sec = timeout;
        tval.tv_usec = 0;

        result = select(sock+1, &rset, &wset, NULL, \
                timeout > 0 ? &tval : NULL);
#else
        pollfds.fd = sock;
        pollfds.events = POLLIN | POLLOUT;
        result = poll(&pollfds, 1, 1000 * timeout);
#endif

        if (result == 0)
        {
            result = ETIMEDOUT;
            break;
        }
        else if (result < 0)
        {
            result = errno != 0 ? errno : EINTR;
            break;
        }

        len = sizeof(result);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &result, &len) < 0)
        {
            result = errno != 0 ? errno : EACCES;
            break;
        }
    } while (0);

    /*恢复原来的标志位*/
    if (needRestore)
    {
        fcntl(sock, F_SETFL, flags);
    }

    return result;
}

/*
 *根据getnamefunc（getsockname/getpeername）来取sock对应
 *本机或对端的ip
 */
in_addr_t getIpaddr(getnamefunc getname, int sock, \
        char *buff, const int bufferSize)
{
    struct sockaddr_in addr;
    socklen_t addrlen;

    memset(&addr, 0, sizeof(addr));
    addrlen = sizeof(addr);

    if (getname(sock, (struct sockaddr *)&addr, &addrlen) != 0)
    {
        *buff = '\0';
        return INADDR_NONE;
    }

    /*这里应该是addrlen > sizeof(struct sockaddr_in)*/
    if (addrlen > 0)
    {
        /*由网络表达得到字符表达式*/
        if (inet_ntop(AF_INET, &addr.sin_addr, buff, bufferSize) == NULL)
        {
            *buff = '\0';
        }
    }
    else
    {
        *buff = '\0';
    }

    return addr.sin_addr.s_addr;
}

/*根据字符串型的ip拿到主机名*/
char *getHostnameByIp(const char *szIpAddr, char *buff, const int bufferSize)
{
    struct in_addr ip_addr;
    struct hostent *ent;

    if (inet_pton(AF_INET, szIpAddr, &ip_addr) != 1)
    {
        *buff = '\0';
        return buff;
    }

    ent = gethostbyaddr((char *)&ip_addr, sizeof(ip_addr), AF_INET);
    if (ent == NULL || ent->h_name == NULL)
    {
        *buff = '\0';
    }
    else
    {
        snprintf(buff, bufferSize, "%s", ent->h_name);
    }

    return buff;
}

/*根据主机名拿到ip地址，返回值是in_addr_t,字符型的赋给buff*/
in_addr_t getIpaddrByName(const char *name, char *buff, const int bufferSize)
{
    struct in_addr ip_addr;
    struct hostent *ent;
    in_addr_t **addr_list;

    if (inet_pton(AF_INET, name, &ip_addr) == 1)
    {
        if (buff != NULL)
        {
            snprintf(buff, bufferSize, "%s", name);
        }
        return ip_addr.s_addr;
    }

    ent = gethostbyname(name);
    if (ent == NULL)
    {
        return INADDR_NONE;
    }
    addr_list = (in_addr_t **)ent->h_addr_list;
    if (addr_list[0] == NULL)
    {
        return INADDR_NONE;
    }

    memset(&ip_addr, 0, sizeof(ip_addr));
    ip_addr.s_addr = *(addr_list[0]);
    if (buff != NULL)
    {
        if (inet_ntop(AF_INET, &ip_addr, buff, bufferSize) == NULL)
        {
            *buff = '\0';
        }
    }

    return ip_addr.s_addr;
}

/*非阻塞accept*/
int nbaccept(int sock, const int timeout, int *err_no)
{
    struct sockaddr_in inaddr;
    socklen_t sockaddr_len;
    fd_set read_set;
    struct timeval t;
    int result;

    if (timeout > 0)
    {
        t.tv_usec = 0;
        t.tv_sec = timeout;

        FD_ZERO(&read_set);
        FD_SET(sock, &read_set);

        result = select(sock+1, &read_set, NULL, NULL, &t);
        if (result == 0)  //timeout
        {
            *err_no = ETIMEDOUT;
            return -1;
        }
        else if (result < 0) //error
        {
            *err_no = errno != 0 ? errno : EINTR;
            return -1;
        }

        /*
           if (!FD_ISSET(sock, &read_set))
           {
         *err_no = EAGAIN;
         return -1;
         }
         */
    }

    sockaddr_len = sizeof(inaddr);
    result = accept(sock, (struct sockaddr*)&inaddr, &sockaddr_len);
    if (result < 0)
    {
        *err_no = errno != 0 ? errno : EINTR;
    }
    else
    {
        *err_no = 0;
    }

    return result;
}

int socketBind(int sock, const char *bind_ipaddr, const int port)
{
    struct sockaddr_in bindaddr;

    bindaddr.sin_family = AF_INET;
    bindaddr.sin_port = htons(port);
    /*如果为空，则能接收任意进来的信息*/
    if (bind_ipaddr == NULL || *bind_ipaddr == '\0')
    {
        bindaddr.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        /*检查地址是否合法*/
        if (inet_aton(bind_ipaddr, &bindaddr.sin_addr) == 0)
        {
            logError("file: "__FILE__", line: %d, " \
                    "invalid ip addr %s", \
                    __LINE__, bind_ipaddr);
            return EINVAL;
        }
    }

    /*绑定*/
    if (bind(sock, (struct sockaddr*)&bindaddr, sizeof(bindaddr)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "bind port %d failed, " \
                "errno: %d, error info: %s.", \
                __LINE__, port, errno, strerror(errno));
        return errno != 0 ? errno : ENOMEM;
    }

    return 0;
}

int socketServer(const char *bind_ipaddr, const int port, int *err_no)
{
    int sock;
    int result;

    /*创建socket*/
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        *err_no = errno != 0 ? errno : EMFILE;
        logError("file: "__FILE__", line: %d, " \
                "socket create failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return -1;
    }

    /*设置地址重用*/
    result = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &result, sizeof(int))<0)
    {
        *err_no = errno != 0 ? errno : ENOMEM;
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        close(sock);
        return -2;
    }

    /*绑定*/
    if ((*err_no=socketBind(sock, bind_ipaddr, port)) != 0)
    {
        close(sock);
        return -3;
    }

    /*监听的队列为1024*/
    if (listen(sock, 1024) < 0)
    {
        *err_no = errno != 0 ? errno : EINVAL;
        logError("file: "__FILE__", line: %d, " \
                "listen port %d failed, " \
                "errno: %d, error info: %s", \
                __LINE__, port, errno, strerror(errno));
        close(sock);
        return -4;
    }

    *err_no = 0;
    return sock;
}

/*从sock读入数据写到filename文件中*/
int tcprecvfile(int sock, const char *filename, const int64_t file_bytes, \
        const int fsync_after_written_bytes, const int timeout, \
        int64_t *total_recv_bytes)
{
    int fd;
    char buff[FDFS_WRITE_BUFF_SIZE];
    int64_t remain_bytes;
    int recv_bytes;
    int written_bytes;
    int result;
    int flags;
    int count;
    tcprecvdata_exfunc recv_func;

    *total_recv_bytes = 0;
    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    if (flags & O_NONBLOCK)
    {
        recv_func = tcprecvdata_nb_ex;
    }
    else
    {
        recv_func = tcprecvdata_ex;
    }

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    written_bytes = 0;
    remain_bytes = file_bytes;
    while (remain_bytes > 0)
    {
        if (remain_bytes > sizeof(buff))
        {
            recv_bytes = sizeof(buff);
        }
        else
        {
            recv_bytes = remain_bytes;
        }

        result = recv_func(sock, buff, recv_bytes, \
                timeout, &count);
        *total_recv_bytes += count;
        if (result != 0)
        {
            close(fd);
            unlink(filename);
            return result;
        }

        if (write(fd, buff, recv_bytes) != recv_bytes)
        {
            result = errno != 0 ? errno: EIO;
            close(fd);
            unlink(filename);
            return result;
        }

        if (fsync_after_written_bytes > 0)
        {
            written_bytes += recv_bytes;
            if (written_bytes >= fsync_after_written_bytes)
            {
                written_bytes = 0;
                if (fsync(fd) != 0)
                {
                    result = errno != 0 ? errno: EIO;
                    close(fd);
                    unlink(filename);
                    return result;
                }
            }
        }

        remain_bytes -= recv_bytes;
    }

    close(fd);
    return 0;
}

/*跟tcprecvfile区别的就是增加了计算文件的hash值*/
int tcprecvfile_ex(int sock, const char *filename, const int64_t file_bytes, \
        const int fsync_after_written_bytes, \
        unsigned int *hash_codes, const int timeout)
{
    int fd;
    char buff[FDFS_WRITE_BUFF_SIZE];
    int64_t remain_bytes;
    int recv_bytes;
    int written_bytes;
    int result;
    int flags;
    tcprecvdata_exfunc recv_func;

    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    if (flags & O_NONBLOCK)
    {
        recv_func = tcprecvdata_nb_ex;
    }
    else
    {
        recv_func = tcprecvdata_ex;
    }

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    INIT_HASH_CODES4(hash_codes)

        written_bytes = 0;
    remain_bytes = file_bytes;
    while (remain_bytes > 0)
    {
        if (remain_bytes > sizeof(buff))
        {
            recv_bytes = sizeof(buff);
        }
        else
        {
            recv_bytes = remain_bytes;
        }

        if ((result=recv_func(sock, buff, recv_bytes, \
                        timeout, NULL)) != 0)
        {
            close(fd);
            unlink(filename);
            return result;
        }

        if (write(fd, buff, recv_bytes) != recv_bytes)
        {
            result = errno != 0 ? errno: EIO;
            close(fd);
            unlink(filename);
            return result;
        }

        if (fsync_after_written_bytes > 0)
        {
            written_bytes += recv_bytes;
            if (written_bytes >= fsync_after_written_bytes)
            {
                written_bytes = 0;
                if (fsync(fd) != 0)
                {
                    result = errno != 0 ? errno: EIO;
                    close(fd);
                    unlink(filename);
                    return result;
                }
            }
        }

        CALC_HASH_CODES4(buff, recv_bytes, hash_codes)

            remain_bytes -= recv_bytes;
    }

    close(fd);

    FINISH_HASH_CODES4(hash_codes)

        return 0;
}

/*将接收的数据丢弃*/
int tcpdiscard(int sock, const int bytes, const int timeout, \
        int64_t *total_recv_bytes)
{
    char buff[FDFS_WRITE_BUFF_SIZE];
    int remain_bytes;
    int recv_bytes;
    int result;
    int flags;
    int count;
    tcprecvdata_exfunc recv_func;

    *total_recv_bytes = 0;
    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    if (flags & O_NONBLOCK)
    {
        recv_func = tcprecvdata_nb_ex;
    }
    else
    {
        recv_func = tcprecvdata_ex;
    }

    remain_bytes = bytes;
    while (remain_bytes > 0)
    {
        if (remain_bytes > sizeof(buff))
        {
            recv_bytes = sizeof(buff);
        }
        else
        {
            recv_bytes = remain_bytes;
        }

        result = recv_func(sock, buff, recv_bytes, \
                timeout, &count);
        *total_recv_bytes += count;
        if (result != 0)
        {
            return result;
        }

        remain_bytes -= recv_bytes;
    }

    return 0;
}

/*
 *在定义了USE_SENDFILE的情况下使用sendfile函数发送文件，
 *否则用普通的发送函数
 */
int tcpsendfile_ex(int sock, const char *filename, const int64_t file_offset, \
        const int64_t file_bytes, const int timeout)
{
    int fd;
    int64_t send_bytes;
    int result;
    int flags;
#ifdef USE_SENDFILE
#if defined(OS_FREEBSD) || defined(OS_LINUX)
    off_t offset;
#ifdef OS_LINUX
    int64_t remain_bytes;
#endif
#endif
#else
    int64_t remain_bytes;
#endif

    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return errno != 0 ? errno : EACCES;
    }

#ifdef USE_SENDFILE

    if (flags & O_NONBLOCK)
    {
        if (fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) == -1)
        {
            return errno != 0 ? errno : EACCES;
        }
    }

#ifdef OS_LINUX
    /*
       result = 1;
       if (setsockopt(sock, SOL_TCP, TCP_CORK, &result, sizeof(int)) < 0)
       {
       logError("file: "__FILE__", line: %d, " \
       "setsockopt failed, errno: %d, error info: %s.", \
       __LINE__, errno, strerror(errno));
       close(fd);
       return errno != 0 ? errno : EIO;
       }
       */

#define FILE_1G_SIZE    (1 * 1024 * 1024 * 1024)

    result = 0;
    offset = file_offset;
    remain_bytes = file_bytes;
    while (remain_bytes > 0)
    {
        if (remain_bytes > FILE_1G_SIZE)
        {
            send_bytes = sendfile(sock, fd, &offset, FILE_1G_SIZE);
        }
        else
        {
            send_bytes = sendfile(sock, fd, &offset, remain_bytes);
        }

        if (send_bytes <= 0)
        {
            result = errno != 0 ? errno : EIO;
            break;
        }

        remain_bytes -= send_bytes;
    }
#else
#ifdef OS_FREEBSD
    offset = file_offset;
    if (sendfile(fd, sock, offset, file_bytes, NULL, NULL, 0) != 0)
    {
        result = errno != 0 ? errno : EIO;
    }
    else
    {
        result = 0;
    }
#endif
#endif

    if (flags & O_NONBLOCK)  //restore
    {
        if (fcntl(sock, F_SETFL, flags) == -1)
        {
            result = errno != 0 ? errno : EACCES;
        }
    }

    /*在定义USE_SENDFILE的情况，开始返回*/
#ifdef OS_LINUX
    close(fd);
    return result;
#endif

#ifdef OS_FREEBSD
    close(fd);
    return result;
#endif

#endif

    {
        char buff[FDFS_WRITE_BUFF_SIZE];
        int64_t remain_bytes;
        tcpsenddatafunc send_func;

        if (file_offset > 0 && lseek(fd, file_offset, SEEK_SET) < 0)
        {
            result = errno != 0 ? errno : EIO;
            close(fd);
            return result;
        }

        if (flags & O_NONBLOCK)
        {
            send_func = tcpsenddata_nb;
        }
        else
        {
            send_func = tcpsenddata;
        }

        result = 0;
        remain_bytes = file_bytes;
        while (remain_bytes > 0)
        {
            if (remain_bytes > sizeof(buff))
            {
                send_bytes = sizeof(buff);
            }
            else
            {
                send_bytes = remain_bytes;
            }

            if (read(fd, buff, send_bytes) != send_bytes)
            {
                result = errno != 0 ? errno : EIO;
                break;
            }

            if ((result=send_func(sock, buff, send_bytes, \
                            timeout)) != 0)
            {
                break;
            }

            remain_bytes -= send_bytes;
        }
    }

    close(fd);
    return result;
}

/*设置服务器端的socket option*/
int tcpsetserveropt(int fd, const int timeout)
{
    int flags;
    int result;

    struct linger linger;
    struct timeval waittime;

    linger.l_onoff = 1;
    linger.l_linger = timeout * 100;
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, \
                &linger, (socklen_t)sizeof(struct linger)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : ENOMEM;
    }

    waittime.tv_sec = timeout;
    waittime.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                &waittime, (socklen_t)sizeof(struct timeval)) < 0)
    {
        logWarning("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
    }

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                &waittime, (socklen_t)sizeof(struct timeval)) < 0)
    {
        logWarning("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
    }

    /*
       {
       int bytes;
       int size;

       bytes = 0;
       size = sizeof(int);
       if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
       &bytes, (socklen_t *)&size) < 0)
       {
       logError("file: "__FILE__", line: %d, " \
       "getsockopt failed, errno: %d, error info: %s", \
       __LINE__, errno, strerror(errno));
       return errno != 0 ? errno : ENOMEM;
       }
       printf("send buff size: %d\n", bytes);

       if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
       &bytes, (socklen_t *)&size) < 0)
       {
       logError("file: "__FILE__", line: %d, " \
       "getsockopt failed, errno: %d, error info: %s", \
       __LINE__, errno, strerror(errno));
       return errno != 0 ? errno : ENOMEM;
       }
       printf("recv buff size: %d\n", bytes);
       }
       */

    flags = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, \
                (char *)&flags, sizeof(flags)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    if ((result=tcpsetkeepalive(fd, 2 * timeout + 1)) != 0)
    {
        return result;
    }

    return 0;
}

int tcpsetkeepalive(int fd, const int idleSeconds)
{
    int keepAlive;

#ifdef OS_LINUX
    int keepIdle;
    int keepInterval;
    int keepCount;
#endif

    keepAlive = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, \
                (char *)&keepAlive, sizeof(keepAlive)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

#ifdef OS_LINUX
    keepIdle = idleSeconds;
    if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *)&keepIdle, \
                sizeof(keepIdle)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    keepInterval = 10;
    if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *)&keepInterval, \
                sizeof(keepInterval)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    keepCount = 3;
    if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *)&keepCount, \
                sizeof(keepCount)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }
#endif

    return 0;
}

int tcpprintkeepalive(int fd)
{
    int keepAlive;
    socklen_t len;

#ifdef OS_LINUX
    int keepIdle;
    int keepInterval;
    int keepCount;
#endif

    len = sizeof(keepAlive);
    if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, \
                (char *)&keepAlive, &len) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

#ifdef OS_LINUX
    len = sizeof(keepIdle);
    if (getsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *)&keepIdle, \
                &len) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    len = sizeof(keepInterval);
    if (getsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *)&keepInterval, \
                &len) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    len = sizeof(keepCount);
    if (getsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *)&keepCount, \
                &len) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    logInfo("keepAlive=%d, keepIdle=%d, keepInterval=%d, keepCount=%d", 
            keepAlive, keepIdle, keepInterval, keepCount);
#else
    logInfo("keepAlive=%d", keepAlive);
#endif

    return 0;
}

int tcpsetnonblockopt(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "fcntl failed, errno: %d, error info: %s.", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EACCES;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        logError("file: "__FILE__", line: %d, " \
                "fcntl failed, errno: %d, error info: %s.", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EACCES;
    }

    return 0;
}

int tcpsetnodelay(int fd, const int timeout)
{
    int flags;
    int result;

    if ((result=tcpsetkeepalive(fd, 2 * timeout + 1)) != 0)
    {
        return result;
    }

    flags = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, \
                (char *)&flags, sizeof(flags)) < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "setsockopt failed, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EINVAL;
    }

    return 0;
}

/*根据接口名获取ip地址*/
int gethostaddrs(char **if_alias_prefixes, const int prefix_count, \
        char ip_addrs[][IP_ADDRESS_SIZE], const int max_count, int *count)
{
    struct hostent *ent;
    char hostname[128];
    char *alias_prefixes1[1];
    char **true_alias_prefixes;
    int true_count;
    int i;
    int k;
    int sock;
    struct ifreq req;
    struct sockaddr_in *addr;
    int ret;

    *count = 0;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "socket create failed, errno: %d, error info: %s.", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EMFILE;
    }

    if (prefix_count <= 0)
    {
#ifdef OS_FREEBSD
#define IF_NAME_PREFIX    "bge"
#else
#ifdef OS_SUNOS
#define IF_NAME_PREFIX   "e1000g"
#else
#ifdef OS_AIX
#define IF_NAME_PREFIX   "en"
#else
#define IF_NAME_PREFIX   "eth"
#endif
#endif
#endif

        alias_prefixes1[0] = IF_NAME_PREFIX;
        true_count = 1;
        true_alias_prefixes = alias_prefixes1;
    }
    else
    {
        true_count = prefix_count;
        true_alias_prefixes = if_alias_prefixes;
    }

    for (i=0; i<true_count && *count<max_count; i++)
    {
        for (k=0; k<max_count; k++)
        {
            memset(&req, 0, sizeof(req));
            sprintf(req.ifr_name, "%s%d", true_alias_prefixes[i], k);
            /*获取ip地址*/
            ret = ioctl(sock, SIOCGIFADDR, &req);
            if (ret == -1)
            {
                break;
            }

            addr = (struct sockaddr_in*)&req.ifr_addr;
            if (inet_ntop(AF_INET, &addr->sin_addr, ip_addrs[*count], \
                        IP_ADDRESS_SIZE) != NULL)
            {
                (*count)++;
                if (*count >= max_count)
                {
                    break;
                }
            }
        }
    }

    close(sock);
    /*能通过接口获取ip*/
    if (*count > 0)
    {
        return 0;
    }
    /*否则获取主机名，并根据主机名拿ip*/
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        logError("file: "__FILE__", line: %d, " \
                "call gethostname fail, " \
                "error no: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno != 0 ? errno : EFAULT;
    }

    ent = gethostbyname(hostname);
    if (ent == NULL)
    {
        logError("file: "__FILE__", line: %d, " \
                "call gethostbyname fail, " \
                "error no: %d, error info: %s", \
                __LINE__, h_errno, strerror(h_errno));
        return h_errno != 0 ? h_errno : EFAULT;
    }

    k = 0;
    while (ent->h_addr_list[k] != NULL)
    {
        if (*count >= max_count)
        {
            break;
        }

        if (inet_ntop(ent->h_addrtype, ent->h_addr_list[k], \
                    ip_addrs[*count], IP_ADDRESS_SIZE) != NULL)
        {
            (*count)++;
        }

        k++;
    }

    return 0;
}


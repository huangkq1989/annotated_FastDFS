/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * Tracker主线程：
 * 1.初始化日记，读取配置文件
 * 2.初始化base64编码，设置随机数种子
 * 3.读取数据文件进行初始化
 * 4.后台化该进程（包括设置运行本进程用户及其组）
 * 5.注册信号处理函数
 * 6.开启一条线程，开始定时调度同步日记缓冲和检查心跳
 * 8.开启N条线程，每条线程开始处理请求
 * 9.检查信号是否发生，发生开始平稳关闭进程，把数据
 * 写回个文件
 * 10.关闭日记
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "shared_func.h"
#include "pthread_func.h"
#include "logger.h"
#include "fdfs_global.h"
#include "base64.h"
#include "sockopt.h"
#include "sched_thread.h"
#include "tracker_types.h"
#include "tracker_mem.h"
#include "tracker_service.h"
#include "tracker_global.h"
#include "tracker_func.h"

#ifdef WITH_HTTPD
#include "tracker_httpd.h"
#include "tracker_http_check.h"
#endif

#if defined(DEBUG_FLAG)

#if defined(OS_LINUX)
#include "linux_stack_trace.h"
static bool bSegmentFault = false;
#endif

#include "tracker_dump.h"

#endif

static bool bTerminateFlag = false;

/*接收到QUIT信号的处理函数*/
static void sigQuitHandler(int sig);
static void sigHupHandler(int sig);
static void sigUsrHandler(int sig);

#if defined(DEBUG_FLAG)
#if defined(OS_LINUX)
static void sigSegvHandler(int signum, siginfo_t *info, void *ptr);
#endif

static void sigDumpHandler(int sig);
#endif

#define SCHEDULE_ENTRIES_COUNT 2

int main(int argc, char *argv[])
{
    char *conf_filename;
    char bind_addr[IP_ADDRESS_SIZE];
    int result;
    int sock;

    pthread_t *tids;

    /*调度事件的线程id*/
    pthread_t schedule_tid;

    struct sigaction act;
    /*调度对象，刷洗日记缓冲，同步数据*/
    ScheduleEntry scheduleEntries[SCHEDULE_ENTRIES_COUNT];
    ScheduleArray scheduleArray;

    if (argc < 2)
    {
        printf("Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    /*初始化日志*/
    log_init();

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
    if (getExeAbsoluteFilename(argv[0], g_exe_name, \
                sizeof(g_exe_name)) == NULL)
    {
        log_destroy();
        return errno != 0 ? errno : ENOENT;
    }
#endif


    /*加载配置文件*/
    conf_filename = argv[1];
    memset(bind_addr, 0, sizeof(bind_addr));
    if ((result=tracker_load_from_conf_file(conf_filename, \
                    bind_addr, sizeof(bind_addr))) != 0)
    {
        log_destroy();
        return result;
    }

    /*初始化base64编码，设置随机数种子*/
    base64_init_ex(&g_base64_context, 0, '-', '_', '.');
    if ((result=set_rand_seed()) != 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "set_rand_seed fail, program exit!", __LINE__);
        return result;
    }

    /***
     * 初始化组成员，根据如下文件：
     * "storage_groups.dat"
     * "storage_servers.dat"
     * "storage_changelog.dat"
     * "storage_sync_timestamp.dat"
     * 加载组信息，存储服务器信息.
     * 在首次的时候，是没有这些数据的
     ***/
    if ((result=tracker_mem_init()) != 0)
    {
        log_destroy();
        return result;
    }

    /*开始监听,没开始accept*/
    sock = socketServer(bind_addr, g_server_port, &result);
    if (sock < 0)
    {
        log_destroy();
        return result;
    }

    /*设置SO_LINGER/SNDTIMEO/RCVTIMEO*/
    if ((result=tcpsetserveropt(sock, g_fdfs_network_timeout)) != 0)
    {
        log_destroy();
        return result;
    }

    /*使之成为后台进程，umask(0)及dup2其实应该放到daemon_init()里面的，
     *这些属于daemon化的一部分. 
     */
    daemon_init(true);
    umask(0);

    if (dup2(g_log_context.log_fd, STDOUT_FILENO) < 0 || \
            dup2(g_log_context.log_fd, STDERR_FILENO) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call dup2 fail, errno: %d, error info: %s, " \
                "program exit!", __LINE__, errno, strerror(errno));
        g_continue_flag = false;
        return errno;
    }

    /*准备启动服务了：这里只是初始化锁而已*/
    if ((result=tracker_service_init()) != 0)
    {
        log_destroy();
        return result;
    }

    g_tracker_thread_count = 0;

    /*设置信号处理函数*/
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);

    act.sa_handler = sigUsrHandler;
    if(sigaction(SIGUSR1, &act, NULL) < 0 || \
            sigaction(SIGUSR2, &act, NULL) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call sigaction fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }

    act.sa_handler = sigHupHandler;
    if(sigaction(SIGHUP, &act, NULL) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call sigaction fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }

    act.sa_handler = SIG_IGN;
    if(sigaction(SIGPIPE, &act, NULL) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call sigaction fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }

    act.sa_handler = sigQuitHandler;
    if(sigaction(SIGINT, &act, NULL) < 0 || \
            sigaction(SIGTERM, &act, NULL) < 0 || \
            sigaction(SIGABRT, &act, NULL) < 0 || \
            sigaction(SIGQUIT, &act, NULL) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call sigaction fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }

#if defined(DEBUG_FLAG)
#if defined(OS_LINUX)
    /*设置段错误的处理函数*/
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = sigSegvHandler;
    act.sa_flags = SA_SIGINFO;   /*说明信号处理函数有三个参数*/
    if (sigaction(SIGSEGV, &act, NULL) < 0 || \
            sigaction(SIGABRT, &act, NULL) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call sigaction fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }
#endif

    /*在调试的情况下，会覆盖前面的sigUsrHandler*/
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = sigDumpHandler;
    if(sigaction(SIGUSR1, &act, NULL) < 0 || \
            sigaction(SIGUSR2, &act, NULL) < 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "call sigaction fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }
#endif

#ifdef WITH_HTTPD
    if (!g_http_params.disabled)
    {
        /**
         * 启动http服务器（新线程），接收下载请求，合法的话，
         * 会重定向到对应的storage服务器
         **/
        if ((result=tracker_httpd_start(bind_addr)) != 0)
        {
            logCrit("file: "__FILE__", line: %d, " \
                    "tracker_httpd_start fail, program exit!", \
                    __LINE__);
            return result;
        }

    }

    /*启动新线程连接各storage，检查http服务是否正常*/
    if ((result=tracker_http_check_start()) != 0)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "tracker_http_check_start fail, " \
                "program exit!", __LINE__);
        return result;
    }
#endif

    /*设置运行用户和组*/
    if ((result=set_run_by(g_run_by_group, g_run_by_user)) != 0)
    {
        log_destroy();
        return result;
    }

    /* *
     * 设置调度事件，每个ScheduleArray的所有ScheduleEntry
     * 只会由新启动的一条线程处理
     * */
    scheduleArray.entries = scheduleEntries;
    scheduleArray.count = SCHEDULE_ENTRIES_COUNT;

    memset(scheduleEntries, 0, sizeof(scheduleEntries));
    /*刷洗日记缓冲区，该系统的日志有缓冲，由该调度体处理*/
    scheduleEntries[0].id = 1;
    scheduleEntries[0].time_base.hour = TIME_NONE;
    scheduleEntries[0].time_base.minute = TIME_NONE;
    scheduleEntries[0].interval = g_sync_log_buff_interval;
    scheduleEntries[0].task_func = log_sync_func;
    scheduleEntries[0].func_args = &g_log_context;


    /* **
     * 心跳处理，定时检查，如果检查时刻减去上一次的报告大于
     * 额定值，则超时，认为存储成员服务宕机了
     * */
    scheduleEntries[1].id = 2;
    scheduleEntries[1].time_base.hour = TIME_NONE;
    scheduleEntries[1].time_base.minute = TIME_NONE;
    scheduleEntries[1].interval = g_check_active_interval;
    scheduleEntries[1].task_func = tracker_mem_check_alive;
    scheduleEntries[1].func_args = NULL;

    /*启动调度,g_continuue_flag为调度是否继续的标志*/
    if ((result=sched_start(&scheduleArray, &schedule_tid, \
                    g_thread_stack_size, &g_continue_flag)) != 0)
    {
        log_destroy();
        return result;
    }

    /*启动最大连接数条线程*/
    tids = (pthread_t *)malloc(sizeof(pthread_t) * g_max_connections);
    if (tids == NULL)
    {
        logCrit("file: "__FILE__", line: %d, " \
                "malloc fail, errno: %d, error info: %s", \
                __LINE__, errno, strerror(errno));
        return errno;
    }

    g_tracker_thread_count = g_max_connections;
    /*启动的工作线程会调度tracker_thread_entrance*/
    if ((result=create_work_threads(&g_tracker_thread_count, \
                    tracker_thread_entrance, (void *)((long)sock), tids, \
                    g_thread_stack_size)) != 0)
    {
        free(tids);
        log_destroy();
        return result;
    }

    /*设置日志是有缓冲的*/
    log_set_cache(true);

    g_thread_kill_done = false;
    bTerminateFlag = false;
    /*在没接收到出错信号和退出信号时，一直循环*/
    while (g_continue_flag)
    {
        sleep(1);

        /*相应信号函数把这个变量置为真时*/
        if (bTerminateFlag)
        {
            /*设置后工作者线程不会继续循环下去了*/
            g_continue_flag = false;

            /*定时调度工作还在继续*/
            if (g_schedule_flag)
            {
                pthread_kill(schedule_tid, SIGINT);
            }
            kill_work_threads(tids, g_max_connections);
            g_thread_kill_done = true;

#ifdef WITH_HTTPD
            /*停止http check*/
            tracker_http_check_stop();
#endif

            break;
        }
    }

    /*工作线程未被全部杀死或调度线程还在处理*/
    while ((g_tracker_thread_count != 0) || g_schedule_flag)
    {

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
        if (bSegmentFault)
        {
            /*段错误，休眠5秒，然后就不等了*/
            sleep(5);
            break;
        }
#endif

        usleep(50000);// 50ms
    }


    /**
     * 执行与tracker_mem_init()相反的操作
     * 写回数据到文件,释放内存
     **/
    tracker_mem_destroy();
    /**
     *只是销毁:
     *    tracker_thread_lock
     *    lb_thread_lock
     **/
    tracker_service_destroy();
    free(tids);

    logInfo("exit nomally.\n");
    /*销毁日记*/
    log_destroy();

    return 0;
}

#if defined(DEBUG_FLAG)
#if defined(OS_LINUX)
/*当段错误发生时，会一直循环调用这个函数*/
static void sigSegvHandler(int signum, siginfo_t *info, void *ptr)
{
    bSegmentFault = true;

    /*避免多次打印*/
    if (!bTerminateFlag)
    {
        /*终止服务*/
        bTerminateFlag = true;
        logCrit("file: "__FILE__", line: %d, " \
                "catch signal %d, program exiting...", \
                __LINE__, signum);

        signal_stack_trace_print(signum, info, ptr);
    }
}
#endif

static void sigDumpHandler(int sig)
{
    /*设置为静态的,嵌套调用*/
    static bool bDumpFlag = false;
    char filename[256];

    if (bDumpFlag)
    {
        return;
    }

    bDumpFlag = true;

    snprintf(filename, sizeof(filename), 
            "%s/logs/tracker_dump.log", g_fdfs_base_path);
    fdfs_dump_tracker_global_vars_to_file(filename);

    bDumpFlag = false;
}

#endif

static void sigQuitHandler(int sig)
{
    if (!bTerminateFlag)
    {
        /*终止服务*/
        bTerminateFlag = true;
        logCrit("file: "__FILE__", line: %d, " \
                "catch signal %d, program exiting...", \
                __LINE__, sig);
    }
}

static void sigHupHandler(int sig)
{
    logInfo("file: "__FILE__", line: %d, " \
            "catch signal %d, ignore it", __LINE__, sig);
}

static void sigUsrHandler(int sig)
{
    logInfo("file: "__FILE__", line: %d, " \
            "catch signal %d, ignore it", __LINE__, sig);
}

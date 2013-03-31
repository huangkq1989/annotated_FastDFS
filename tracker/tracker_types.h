/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 此文件定义了FastDFS tracker需要用的一些结构体：
 *  1. group信息 
 *  2. storage信息
 *  3. 处理请求和发起请求需要的信息
 * 和一些常量，如最多64个组
 *
 **************************************************/

/**
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
 **/

//tracker_types.h

#ifndef _TRACKER_TYPES_H_
#define _TRACKER_TYPES_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "fdfs_define.h"

#define FDFS_ONE_MB	(1024 * 1024)

#define FDFS_GROUP_NAME_MAX_LEN		16
#define FDFS_MAX_SERVERS_EACH_GROUP	32
#define FDFS_MAX_GROUPS			64
#define FDFS_MAX_TRACKERS		16

#define FDFS_MAX_META_NAME_LEN		 64
#define FDFS_MAX_META_VALUE_LEN		256

#define FDFS_FILE_PREFIX_MAX_LEN	16
#define FDFS_FILE_PATH_LEN		10
#define FDFS_FILENAME_BASE64_LENGTH     27

#define FDFS_VERSION_SIZE		6

//status order is important!
#define FDFS_STORAGE_STATUS_INIT	  0
#define FDFS_STORAGE_STATUS_WAIT_SYNC	  1   /*等待同步*/
#define FDFS_STORAGE_STATUS_SYNCING	  2       /*正在同步*/
/*storage的ip变了，因为Tracker是根据ip来定位的，所以ip变了要进行调整*/
#define FDFS_STORAGE_STATUS_IP_CHANGED    3   
#define FDFS_STORAGE_STATUS_DELETED	  4
#define FDFS_STORAGE_STATUS_OFFLINE	  5
#define FDFS_STORAGE_STATUS_ONLINE	  6       /*在线，但未激活*/
#define FDFS_STORAGE_STATUS_ACTIVE	  7       /*在线，已经激活*/
#define FDFS_STORAGE_STATUS_NONE	 99

//which group to upload file
/*上传文件选组的策略*/
#define FDFS_STORE_LOOKUP_ROUND_ROBIN	0  //round robin
#define FDFS_STORE_LOOKUP_SPEC_GROUP	1  //specify group
#define FDFS_STORE_LOOKUP_LOAD_BALANCE	2  //load balance

//which server to upload file
/*上传文件选storage的策略*/
#define FDFS_STORE_SERVER_ROUND_ROBIN	0  //round robin
#define FDFS_STORE_SERVER_FIRST_BY_IP	1  //the first server order by ip
#define FDFS_STORE_SERVER_FIRST_BY_PRI	2  //the first server order by priority

//which server to download file
#define FDFS_DOWNLOAD_SERVER_ROUND_ROBIN	0  //round robin
#define FDFS_DOWNLOAD_SERVER_SOURCE_FIRST	1  //the source server

//which path to upload file
#define FDFS_STORE_PATH_ROUND_ROBIN	0  //round robin
/*这里负载均衡时跟据连接数还是空间呢？*/
#define FDFS_STORE_PATH_LOAD_BALANCE	2  //load balance

//the mode of the files distributed to the data path
#define FDFS_FILE_DIST_PATH_ROUND_ROBIN	0  //round robin
#define FDFS_FILE_DIST_PATH_RANDOM	1  //random

//http check alive type
#define FDFS_HTTP_CHECK_ALIVE_TYPE_TCP  0  //tcp
#define FDFS_HTTP_CHECK_ALIVE_TYPE_HTTP 1  //http

#define FDFS_DOWNLOAD_TYPE_TCP	0  //tcp
#define FDFS_DOWNLOAD_TYPE_HTTP	1  //http

/*每层目录的个数，共两层 */
#define FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT   100

#define FDFS_DOMAIN_NAME_MAX_SIZE	128

/*存储服务器的简要信息，状态和IP地址*/
typedef struct
{
    char status;     /*online, active之类的*/
    char ip_addr[IP_ADDRESS_SIZE];
} FDFSStorageBrief;

/**
 * Storage组的状态
 * base path是存储数据的directory，在配置文件中配置
 * 同一个组的所有storage是在同一端口监听
 */
typedef struct
{
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int64_t free_mb;  //free disk storage in MB
    int count;        //server count
    int storage_port; //storage server port
    int storage_http_port; //storage server http port
    int active_count; //active server count
    int current_write_server; //current server index to upload file
    int store_path_count;  //store base path count of each storage server
    int subdir_count_per_path;  /*每个base path的子目录*/
} FDFSGroupStat;

/*storage的状态*/
typedef struct
{
    /* following count stat by source server,
       not including synced count
       */
    int64_t total_upload_count;
    int64_t success_upload_count;
    int64_t total_set_meta_count;
    int64_t success_set_meta_count;
    int64_t total_delete_count;
    int64_t success_delete_count;
    int64_t total_download_count;
    int64_t success_download_count;
    int64_t total_get_meta_count;
    int64_t success_get_meta_count;
    int64_t total_create_link_count;
    int64_t success_create_link_count;
    int64_t total_delete_link_count;
    int64_t success_delete_link_count;

    /* last update timestamp as source server, 
       current server' timestamp
       */
    time_t last_source_update;

    /* last update timestamp as dest server, 
       current server' timestamp
       */
    time_t last_sync_update;

    /* last syned timestamp, 
       source server's timestamp
       上次同步到的时间戳
       */
    time_t last_synced_timestamp;

    /* last heart beat time */
    /*上次接收到心跳包的时间*/
    time_t last_heart_beat_time;
} FDFSStorageStat;

/*持久化用到的结构体*/
typedef struct
{
    char sz_total_upload_count[8];
    char sz_success_upload_count[8];
    char sz_total_set_meta_count[8];
    char sz_success_set_meta_count[8];
    char sz_total_delete_count[8];
    char sz_success_delete_count[8];
    char sz_total_download_count[8];
    char sz_success_download_count[8];
    char sz_total_get_meta_count[8];
    char sz_success_get_meta_count[8];
    char sz_total_create_link_count[8];
    char sz_success_create_link_count[8];
    char sz_total_delete_link_count[8];
    char sz_success_delete_link_count[8];
    char sz_last_source_update[8];
    char sz_last_sync_update[8];
    char sz_last_synced_timestamp[8];
    char sz_last_heart_beat_time[8];
} FDFSStorageStatBuff;

typedef struct StructFDFSStorageDetail
{
    char status;
    bool dirty;      /*c是没有bool的，bool 是 char*/
    char ip_addr[IP_ADDRESS_SIZE];
    char domain_name[FDFS_DOMAIN_NAME_MAX_SIZE];
    char version[FDFS_VERSION_SIZE];  /*版本,不同的版本协议有些变动，Tracker可以处理不同版本的storage*/

    /**!!!!**/
    /**
     * 当一台全新的storage加入时，只会有一台storage（假设为A）将全部的现有的数据
     * 发送给这台新加入的storage,而psync_src_server指向的就是机器是storage A
     **/
    /**!!!!**/
    struct StructFDFSStorageDetail *psync_src_server;
    time_t sync_until_timestamp;

    time_t up_time;
    int64_t total_mb;  //total disk storage in MB
    int64_t free_mb;  //free disk storage in MB
    /*changelog是记录storage的变更的，主要是记录ip变化和storage删除*/
    /*storage记录自己的偏移量，以后从偏移量后开始把数据取回去*/
    int64_t changelog_offset;  //changelog file offset

    int64_t *path_total_mbs; //total disk storage in MB
    int64_t *path_free_mbs;  //free disk storage in MB

    int store_path_count;  //store base path count of each storage server
    int subdir_count_per_path;
    int upload_priority; //storage upload priority

    int storage_port;
    int storage_http_port; //storage http server port

    int current_write_path; //current write path index

    int *ref_count;   //group/storage servers referer count
    int chg_count;    //current server changed counter
    FDFSStorageStat stat;    

#ifdef WITH_HTTPD
    int http_check_last_errno;
    int http_check_last_status;
    int http_check_fail_count;
    char http_check_error_info[256];
#endif
} FDFSStorageDetail;

/*storage group 的信息*/
typedef struct
{
    bool dirty; /*在从新分配时，如果旧的还有被引用，就标记为脏: 查找 dirty = */
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int64_t free_mb;  //free disk storage in MB 所有的storage组合起来的空闲
    int alloc_size;   //为group中的storage预分配空间
    int count;    //total server count
    int active_count; //active server count
    int storage_port;
    int storage_http_port; //storage http server port
    FDFSStorageDetail *all_servers;
    FDFSStorageDetail **sorted_servers;  //order by ip addr
    FDFSStorageDetail **active_servers;  //order by ip addr
    FDFSStorageDetail *pStoreServer;  //for upload priority mode

#ifdef WITH_HTTPD
    FDFSStorageDetail **http_servers;  //order by ip addr
    int http_server_count; //http server count
    int current_http_server;
#endif

    int current_read_server;
    int current_write_server;

    int store_path_count;  //store base path count of each storage server

    /* subdir_count * subdir_count directories will be auto created
       under each store_path (disk) of the storage servers
       */
    int subdir_count_per_path;

    int **last_sync_timestamps;//row for src storage, col for dest storage

    int *ref_count;  //groups referer count
    int chg_count;   //current group changed count 
    time_t last_source_update;
    time_t last_sync_update;
} FDFSGroupInfo;

/* 描述所有的组的信息，
 * 并且存放当前操作的组的相关信息
 */
typedef struct
{
    /**
     * 分配多少个FDFSGroupInfo结构,启动时会分配两个，
     * 如果读取storage_groups.dat发现有多于2个（即
     * 下一字段count大于2），会重新分配
     */
    int alloc_size;  /*被设置为2，应该一个时用于读，一个用于写*/

    /*所有组的信息*/
    /*共有多少个组*/
    int count;  //group count
    /*各组的信息*/
    FDFSGroupInfo *groups;
    FDFSGroupInfo **sorted_groups; //order by group_name

    /*以下是定位策略*/
    FDFSGroupInfo *pStoreGroup;  //the group to store uploaded files
    int current_write_group;  //current group index to upload file
    byte store_lookup;  //store to which group
    byte store_server;  //store to which server
    byte download_server; //download from which server
    byte store_path;  //store to which path
    char store_group[FDFS_GROUP_NAME_MAX_LEN + 1];  /*这是当前操作组的名称*/
} FDFSGroups;

/*tracker 作为服务器端时，客户端（storage/client）需要维护的信息*/
typedef struct
{
    int sock;    /*请求用的是哪个socket fd*/
    int port;    /*请求用的是哪个端口，虽然根据sock能解析处理，但直接存储访问更快*/
    char ip_addr[IP_ADDRESS_SIZE]; /*请求用的是哪个ip*/
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1]; /*请求的storage所属的组名*/
} TrackerServerInfo;

/*tracker在处理连接时tracker需要维护的信息*/
typedef struct
{
    int sock;
    int storage_port;
    char ip_addr[IP_ADDRESS_SIZE];
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    FDFSGroupInfo *pGroup;
    FDFSStorageDetail *pStorage;
    FDFSGroupInfo *pAllocedGroups;		//for free
    FDFSStorageDetail *pAllocedStorages;	//for free
} TrackerClientInfo;

/*storage作为客户时，建立连接其需要的信息*/
typedef struct
{
    int sock;
    char ip_addr[IP_ADDRESS_SIZE];
    char tracker_client_ip[IP_ADDRESS_SIZE];
} StorageClientInfo;

/*元数据*/
typedef struct
{
    char name[FDFS_MAX_META_NAME_LEN + 1];  //key
    char value[FDFS_MAX_META_VALUE_LEN + 1]; //value
} FDFSMetaData;

/*新增storage请求同步源头机器的结构体*/
typedef struct
{
    /*发起请求的storage所在组组名*/
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    /*发起请求的storage的ip*/
    char ip_addr[IP_ADDRESS_SIZE];
    /*发送数据给发起请求的storage的源机器的ip*/
    char sync_src_ip_addr[IP_ADDRESS_SIZE];
} FDFSStorageSync;

/*存储服务器加入组时发送的信息*/
typedef struct
{
    int storage_port;
    int storage_http_port;
    int store_path_count;
    int subdir_count_per_path;
    /*上传优先级*/
    int upload_priority;
    /*启动时间*/
    int up_time;   //storage service started timestamp
    /*Tracker能处理不同版本的Storage*/
    char version[FDFS_VERSION_SIZE];   //storage version
    /*域名*/
    char domain_name[FDFS_DOMAIN_NAME_MAX_SIZE];
    /*存储服务器是否已经初始化*/
    char init_flag;
    /*状态*/
    signed char status;
} FDFSStorageJoinBody;

#endif


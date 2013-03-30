/**************************************************
 * 中文注释by: huangkq1989
 * email: huangkq1989@gmail.com
 *
 * 启动Http请求下载服务,使用库libevent的http处理函数
 * 接收下载请求后检查请求是否合法，然后查询文件在哪
 * 一storage,然后发送重定向包
 *
 **************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <event.h>
#include <evhttp.h>
#include "logger.h"
#include "tracker_global.h"
#include "tracker_mem.h"
#include "tracker_proto.h"
#include "http_func.h"
#include "tracker_httpd.h"

/*libevent的buffer结构*/
static struct evbuffer *ev_buf = NULL;
/*用于等待http服务的线程完成初始化*/
static int http_start_status = 0;

static void generic_handler(struct evhttp_request *req, void *arg)
{
#define HTTPD_MAX_PARAMS   32
	char *url;
	char *file_id;
	char uri[256];
	char szPortPart[16];
	char redirect_url[512];
	int url_len;
	int uri_len;
	int domain_len;
	KeyValuePair params[HTTPD_MAX_PARAMS];
	int param_count;
	char *p;
	char *group_name;
	char *filename;
	FDFSGroupInfo *pGroup;
	FDFSStorageDetail *ppStoreServers[FDFS_MAX_SERVERS_EACH_GROUP];
	int filename_len;
	int server_count;

    /*检查请求url是否合法*/
	url = (char *)evhttp_request_uri(req);
	url_len = strlen(url);
	if (url_len < 16)
	{
		evhttp_send_reply(req, HTTP_BADREQUEST, "Bad request", ev_buf);
		return;
	}

	if (strncasecmp(url, "http://", 7) == 0)
	{
		p = strchr(url+7, '/');
		if (p == NULL)
		{
			evhttp_send_reply(req, HTTP_BADREQUEST, \
					"Bad request", ev_buf);
			return;
		}

		uri_len = url_len - (p - url);
		url = p;
	}
	else
	{
		uri_len = url_len;
	}

	if (uri_len + 1 >= sizeof(uri))
	{
		evhttp_send_reply(req, HTTP_BADREQUEST, "Bad request", ev_buf);
		return;
	}

	if (*url != '/')
	{
		*uri = '/';
		memcpy(uri+1, url, uri_len+1);
		uri_len++;
	}
	else
	{
		memcpy(uri, url, uri_len+1);
	}

    /*解析http参数*/
	param_count = http_parse_query(url, params, HTTPD_MAX_PARAMS);
	file_id = url;
	if (strlen(file_id) < 22)
	{
		evhttp_send_reply(req, HTTP_BADREQUEST, "Bad request", ev_buf);
		return;
	}

	if (*file_id == '/')
	{
		file_id++;
	}

    /*防盗链*/
	if (g_http_params.anti_steal_token)
	{
		char *token;
		char *ts;
		int timestamp;

        /*获取请求中token的参数*/
		token = fdfs_http_get_parameter("token", params, param_count);
		ts = fdfs_http_get_parameter("ts", params, param_count);
		if (token == NULL || ts == NULL)
		{
			evhttp_send_reply(req, HTTP_BADREQUEST, \
					"Bad request", ev_buf);
			return;
		}

		timestamp = atoi(ts);
        /*检查token是否合法*/
		if (fdfs_http_check_token(&g_http_params.anti_steal_secret_key,\
			file_id, timestamp, token, \
			g_http_params.token_ttl) != 0)
		{
            /*token不合法，且响应不合法的字符不为空*/
			if (*(g_http_params.token_check_fail_content_type))
			{
				evbuffer_add(ev_buf, \
				g_http_params.token_check_fail_buff.buff, \
				g_http_params.token_check_fail_buff.length);

				evhttp_add_header(req->output_headers, \
				"Content-Type", \
				g_http_params.token_check_fail_content_type);

				evhttp_send_reply(req, HTTP_OK, "OK", ev_buf);
			}
			else
			{
				evhttp_send_reply(req, HTTP_BADREQUEST, \
						"Bad request", ev_buf);
			}

			return;
		}
	}

	group_name = file_id;
	filename = strchr(file_id, '/');
	if (filename == NULL)
	{
		evhttp_send_reply(req, HTTP_BADREQUEST, "Bad request", ev_buf);
		return;
	}

	*filename = '\0';
	filename++;  //skip '/'
	filename_len = strlen(filename);
    /*获取存有该文件的storage*/
	if (tracker_mem_get_storage_by_filename( \
			TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, \
			FDFS_DOWNLOAD_TYPE_HTTP, group_name, filename, \
			filename_len, &pGroup, \
			ppStoreServers, &server_count) != 0)
	{
		evhttp_send_reply(req, HTTP_BADREQUEST, "Bad request", ev_buf);
		return;
	}

	if (pGroup->storage_http_port == 80)
	{
		*szPortPart = '\0';
	}
	else
	{
		sprintf(szPortPart, ":%d", pGroup->storage_http_port);
	}

    /*把请求重定向，然后浏览器会连接storage*/
	domain_len = sprintf(redirect_url, "http://%s%s", \
		*(ppStoreServers[0]->domain_name) != '\0' ? \
		ppStoreServers[0]->domain_name : ppStoreServers[0]->ip_addr, \
		szPortPart);
	memcpy(redirect_url + domain_len, uri, uri_len);
	*(redirect_url + domain_len + uri_len) = '\0';

	evhttp_add_header(req->output_headers, "Connection", "close");
	evhttp_add_header(req->output_headers, "Location", redirect_url);
	evhttp_send_reply(req, HTTP_MOVETEMP, "Found", ev_buf);
}

/*线程入口函数*/
static void *httpd_entrance(void *arg)
{
	char *bind_addr;
	struct evhttp *httpd;

	bind_addr = (char *)arg;
	if (*bind_addr == '\0')
	{
		bind_addr = "0.0.0.0";
	}
	event_init();
    /*启动evhttp*/
	httpd = evhttp_start(bind_addr, g_http_params.server_port);
	if (httpd == NULL)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"evhttp_start fail, server port=%d, " \
			"errno: %d, error info: %s", \
			__LINE__, g_http_params.server_port, \
			errno, strerror(errno));
		http_start_status = errno != 0 ? errno : EACCES;
		return NULL;
	}

	http_start_status = 0;
    /*设置请求回调函数*/
	evhttp_set_gencb(httpd, generic_handler, NULL);

    /*进入请求处理循环*/
	event_dispatch();
	evhttp_free(httpd);
	return NULL;
}

int tracker_httpd_start(const char *bind_addr)
{
	int result;
	pthread_t tid;

    /*Happy Fish 忘了释放ev_buf了 */
	ev_buf = evbuffer_new();
	if (ev_buf == NULL)
	{
		result = errno != 0 ? errno : ENOMEM;
		logCrit("file: "__FILE__", line: %d, " \
			"call evbuffer_new fail, errno: %d, error info: %s", \
			__LINE__, result, strerror(result));
		return result;
	}

	http_start_status = -1;
    /*启动线程*/
	if ((result=pthread_create(&tid, NULL, httpd_entrance, \
			(void *)bind_addr)) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"create thread failed, errno: %d, error info: %s", \
			__LINE__, result, strerror(result));
		return result;
	}

	while (http_start_status == -1)
	{
		sleep(1);
	}

	return http_start_status;
}


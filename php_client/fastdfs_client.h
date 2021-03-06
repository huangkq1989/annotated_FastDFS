#ifndef FASTDFS_CLIENT_H
#define FASTDFS_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PHP_WIN32
#define PHP_FASTDFS_API __declspec(dllexport)
#else
#define PHP_FASTDFS_API
#endif

PHP_MINIT_FUNCTION(fastdfs_client);
PHP_RINIT_FUNCTION(fastdfs_client);
PHP_MSHUTDOWN_FUNCTION(fastdfs_client);
PHP_RSHUTDOWN_FUNCTION(fastdfs_client);
PHP_MINFO_FUNCTION(fastdfs_client);

ZEND_FUNCTION(fastdfs_active_test);
ZEND_FUNCTION(fastdfs_connect_server);
ZEND_FUNCTION(fastdfs_disconnect_server);
ZEND_FUNCTION(fastdfs_get_last_error_no);
ZEND_FUNCTION(fastdfs_get_last_error_info);

ZEND_FUNCTION(fastdfs_tracker_get_connection);
ZEND_FUNCTION(fastdfs_tracker_make_all_connections);
ZEND_FUNCTION(fastdfs_tracker_close_all_connections);
ZEND_FUNCTION(fastdfs_tracker_list_groups);
ZEND_FUNCTION(fastdfs_tracker_query_storage_store);
ZEND_FUNCTION(fastdfs_tracker_query_storage_update);
ZEND_FUNCTION(fastdfs_tracker_query_storage_fetch);
ZEND_FUNCTION(fastdfs_tracker_query_storage_list);
ZEND_FUNCTION(fastdfs_tracker_query_storage_update1);
ZEND_FUNCTION(fastdfs_tracker_query_storage_fetch1);
ZEND_FUNCTION(fastdfs_tracker_query_storage_list1);
ZEND_FUNCTION(fastdfs_storage_upload_by_filename);
ZEND_FUNCTION(fastdfs_storage_upload_by_filename1);
ZEND_FUNCTION(fastdfs_storage_upload_by_filebuff);
ZEND_FUNCTION(fastdfs_storage_upload_by_filebuff1);
ZEND_FUNCTION(fastdfs_storage_delete_file);
ZEND_FUNCTION(fastdfs_storage_delete_file1);
ZEND_FUNCTION(fastdfs_storage_download_file_to_buff);
ZEND_FUNCTION(fastdfs_storage_download_file_to_buff1);
ZEND_FUNCTION(fastdfs_storage_download_file_to_file);
ZEND_FUNCTION(fastdfs_storage_download_file_to_file1);
ZEND_FUNCTION(fastdfs_storage_set_metadata);
ZEND_FUNCTION(fastdfs_storage_set_metadata1);
ZEND_FUNCTION(fastdfs_storage_get_metadata);
ZEND_FUNCTION(fastdfs_storage_get_metadata1);
ZEND_FUNCTION(fastdfs_http_gen_token);
ZEND_FUNCTION(fastdfs_get_file_info);
ZEND_FUNCTION(fastdfs_gen_slave_filename);
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filename);
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filename1);
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filebuff);
ZEND_FUNCTION(fastdfs_storage_upload_slave_by_filebuff1);

PHP_FASTDFS_API zend_class_entry *php_fdfs_get_ce(void);
PHP_FASTDFS_API zend_class_entry *php_fdfs_get_exception(void);
PHP_FASTDFS_API zend_class_entry *php_fdfs_get_exception_base(int root TSRMLS_DC);

#ifdef __cplusplus
}
#endif

#endif	/* FASTDFS_CLIENT_H */

//// by fanxiushu 2018-03-29
/// 实现的是 OneDrive 的 Microsoft Graph API
/// 这是属于xFsRedir项目工程的一部分，导出的接口函数类似于操作系统的文件操作函数的形式

#pragma once

#ifdef WIN32
typedef  __int64         int64_t;
#endif

struct onedrive_stat_t
{
	bool     is_dir;
	int64_t  size;
	int      attr;
	int64_t  mtime;
	int64_t  ctime;
};
struct onedrive_name_t
{
	char     name[260];
	bool     is_dir;
	int64_t  size;
	int      attr;
	int64_t  mtime;
	int64_t  ctime;
};

typedef int(*ONEDRIVE_REFRESH_TOKEN_CALLBACK)(void* handle, char* access_token, void* param);

int onedrive_authorize_simple(
	int(*func)(const char* url, void* param1, void* param2),
	void* param1, void* param2);

int onedrive_authorize_browser(string& access_token, string& refresh_token,
	int timeout, int(*open_browser)(const char* url, void* param), void* param);

int onedrive_authorize(string& access_token, string& refresh_token);

int onedrive_fromecode_token(const char* code, string& access_token, string& refresh_token, int& expire);
int onedrive_refresh_token(const char* old_refresh_token, string& access_token, string& refresh_token, int& expire);

/////

void* onedrive_create(const char* access_token, int timeout,
	      ONEDRIVE_REFRESH_TOKEN_CALLBACK func, void* param);
void  onedrive_destroy(void* handle);

/////第一个参数handle是 onedrive_create返回值，函数是非线程安全的，要在多线程操作同一个handle，需要同步
//以下函数没特别说明，返回值基本上-2代表服务端返回错误，-1代表网络连接错误.
int onedrive_find_open(void* handle, const char* rpath, const char* findname);
/// -1错误，0 浏览完，1找到
int onedrive_find_next(void* handle, onedrive_name_t* name);

int onedrive_stat(void* handle, const char* rpath, onedrive_stat_t* st);

int onedrive_set_basicinfo(void* handle, const char* rpath,
	int attr, time_t atime, time_t mtime, time_t ctime);

int onedrive_mkdir(void*handle, const char* rpath);

int onedrive_delfile(void* handle, const char* rpath);

int onedrive_rename(void* handle, const char* oldpath, const char* newpath);

int onedrive_offset_read(void* handle, const char* rpath,
	char* buffer, int64_t offset, int length,
	int(*progress)(int curr_trans, int total_len, void* pram), void* param);

int onedrive_upfile(void* handle, const char* rpath, const char* lpath, int timeout,
	int(*progress)(int64_t cur_size, int64_t total_size, void* param), void* param);

int onedrive_set_newsize(void* handle, const char* rpath, int64_t newsize);

int onedrive_ping(void* handle, int timeout);


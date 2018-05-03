/////by fanxiushu 2018-04-11 
//// 
/// 实现的是 OneDrive 的 Microsoft Graph API
/// 这是属于xFsRedir项目工程的一部分，导出的接口函数类似于操作系统的文件操作函数的形式

#include "curl_base.h"
#include <fcntl.h>
#include "onedrive.h"
#include "cJSON.h"

#ifdef WIN32
#include <io.h>
#define lseek64  _lseeki64
#define tell64   _telli64

#define IS_UTF8_PATH    0

////
#else

#define tell64(fd) lseek64(fd, 0, SEEK_CUR)    //获取当前位置

#define IS_UTF8_PATH    1  // linux 平台默认是UTF8  

#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  

#endif

////
static int __onedrive_common_request(struct __onedrive_t* h, const char* method,
	const char* attr, const char* up_data, const char* rpath, cJSON** p_json, int* p_retcode);

///
struct __od_down_url
{
	string   url;
	int64_t  size;
};
////
struct __onedrive_t
{
	CURL*     curl;
	CURLM*    mcurl;
	string    access_token;
	string    http_token; ///
	int       timeout;
	
	/// lock
#ifdef WIN32
	CRITICAL_SECTION  cs;
#else
	pthread_mutex_t   cs;
#endif
	///
	map<string, __od_down_url>  down_urls_cache; ////缓存下载的URL和 rpath 关系，因为每次下载都要做302重定向，为了加快随机下载速度，增加缓存.

	///
	ONEDRIVE_REFRESH_TOKEN_CALLBACK  refresh_token_func;
	void*                            param;

	/// find
	list<onedrive_name_t>  find_files;
	list<onedrive_name_t>::iterator find_iter;
	int is_utf8_path;
};

struct __onedrive_curl_t
{
	__onedrive_t* h; ///
	CURL*  curl;
	struct curl_slist* opt_hdr;
	time_t  last_active_time; ///
							  ////
	string  result; ///

	char*   rd_buf;
	int     rd_off;
	int     rd_len;
	bool    is_abort_trans;
	int(*progress)(int curr_trans, int total_len, void* pram);
	int(*progress2)(int64_t cur_size, int64_t total_size, void* param);
	void* param;
	int64_t file_size;
};

void* onedrive_create(const char* access_token, int timeout,
	       ONEDRIVE_REFRESH_TOKEN_CALLBACK func, void* param )
{
	CURL* curl = NULL;

	curl = curl_easy_init();
	if (!curl) {
		return NULL;
	}
	CURLM* mcurl = curl_multi_init();
	if (!mcurl) {
		curl_easy_cleanup(curl);
		return NULL;
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); /// no signal
	//////
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); //// 302,301自动跳转
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 20); //// 20次重定向

	///timeout
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0); //// second
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout); /// second

	__onedrive_t* ctx = new __onedrive_t;
#ifdef WIN32
	InitializeCriticalSection(&ctx->cs); ////
#else
	pthread_mutex_init(&ctx->cs, NULL);
#endif
	ctx->is_utf8_path = IS_UTF8_PATH; ////
	ctx->timeout = timeout;
	ctx->curl = curl;
	ctx->mcurl = mcurl;
	ctx->refresh_token_func = func;
	ctx->param = param;
	ctx->access_token = access_token;
	ctx->http_token = string("Authorization: Bearer ") + access_token; ///
	ctx->find_files.clear();
	ctx->find_iter = ctx->find_files.end();

	///
	int retcode = 401;
	if (ctx->access_token.length() > 0) {
		cJSON* json = NULL;
		int r = __onedrive_common_request(ctx, "GET", NULL, NULL, "/", &json, &retcode);
		if (json)cJSON_Delete(json);
	}
	/////
	if (retcode == 401 || retcode == 403) { //需要验证
		char token[30 * 1024] = "";

		int r = func(ctx, token, param); //refresh token 

		if (r == 0) {
			ctx->access_token = token;
			ctx->http_token = string("Authorization: Bearer ") + token; ///
			/////
		}
		//////
	}

	///
//	ctx->refresh_token_func = NULL; ///这里为了防止在xFsRedir 不停查询造成查询浪费
	////
	return ctx;
}

void  onedrive_destroy(void* handle)
{
	__onedrive_t* ctx = (__onedrive_t*)handle;
	if (!ctx)return;

	////
	curl_easy_cleanup(ctx->curl);
	curl_multi_cleanup(ctx->mcurl);

	printf("---------*** onedrive_destroy\n");
#ifdef WIN32
	::DeleteCriticalSection(&ctx->cs);
#else
	pthread_mutex_destroy(&ctx->cs);
#endif
	delete ctx;
}

///
static void __onedrive_lock(__onedrive_t* h)
{
#ifdef WIN32
	::EnterCriticalSection(&h->cs);
#else
	pthread_mutex_lock(&h->cs);
#endif
}
static void __onedrive_unlock(__onedrive_t* h)
{
#ifdef WIN32
	::LeaveCriticalSection(&h->cs);
#else
	pthread_mutex_unlock(&h->cs);
#endif
}

//////
static void __onedrive_conv_path(int is_utf8_path, const char* str, char* buf )
{
	char raw_str[16*1024]; ///
	if (!is_utf8_path) {
#ifdef WIN32
		wchar_t t_buf[8192] = L""; ////
		::MultiByteToWideChar(CP_ACP, 0, str, -1, t_buf, sizeof(t_buf) / 2); ///
		::WideCharToMultiByte(CP_UTF8, 0, t_buf, -1, raw_str, sizeof(raw_str), NULL, NULL);
		str = raw_str;
#endif
	}
	/////
	char* pszEncode = buf;
	int i;

	for (i = 0; i < strlen(str); i++) {
		unsigned char c = str[i];
		if (c > 0x20 && c < 0x7f && c != '/') {    // 数字或字母 
			*pszEncode++ = c;
		}
		else {
			sprintf(pszEncode, "%%%.2x", c);
			pszEncode += 3; ///
		}
	}
	//////////
	*pszEncode = '\0';
	//////
}

static int __onedrive_curl_init(__onedrive_curl_t* u, __onedrive_t* h, 
	const char* method, const char* opthdr, const char* url ,
	bool is_add_auth_header=true )
{
	CURL* curl = h->curl; 
	if (!curl) {
		return -1;
	}
	////证书认证，关闭
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	
	///
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); /// no signal
												  //////
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); //// 302,301自动跳转
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 20); //// 20次重定向

	///timeout
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0 ); //// second, 不设置超时
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, h->timeout); /// second
																
	///当 transfer speed 小于 2字节/秒 时 将会在 timeout 秒内 abort 这个 transfer .update download 
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 2L);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, h->timeout );
	
//	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	///////
	//每次都初始化下面三组值，防止后面发生修改
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0); ///
	///
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_READDATA, u);
	///
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, u);
	////

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method); ///

	curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);

	curl_slist* hdrs = NULL;
	const char* conn_alive = "Connection: keep-alive";
//	const char* accept = "Accept: text/html,application/xhtml+xml,application/xml,application/octet-stream,text/plain;q=0.9,*/*;q=0.8";
//	const char* accept_lan = "Accept-Language: zh-cn,zh;q=0.8,en-us;q=0.5,en;q=0.3";
	const char* user_agent = "User-Agent: HTTP.RestAPI-Curl_Library-Fanxiushu-xFsRedir-UserAgent";
//	hdrs = curl_slist_append(hdrs, accept);
//	hdrs = curl_slist_append(hdrs, accept_lan);
	hdrs = curl_slist_append(hdrs, conn_alive);
	hdrs = curl_slist_append(hdrs, user_agent);
	if (is_add_auth_header) {
		hdrs = curl_slist_append(hdrs, h->http_token.c_str()); //// token 
	}
	if (opthdr) {
		const char* s = opthdr, *e;
		while (e = strstr(s, "\r\n")) {
			string line = string(s, e - s);
			hdrs = curl_slist_append(hdrs, line.c_str());
			////
			s = e + 2;
		}
		hdrs = curl_slist_append(hdrs, s);
	}
	if (hdrs) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	}
	/////
	curl_easy_setopt(curl, CURLOPT_URL, url);

	////
	u->curl = curl;
	u->opt_hdr = hdrs; 
	u->rd_buf = NULL;
	u->rd_off = u->rd_len = 0;
	u->progress = NULL; u->progress2 = NULL;
	u->last_active_time = time(0); ////
	u->h = h; 
	u->is_abort_trans = false;
	u->result.clear(); ///

	return 0;
}
static void __onedrive_curl_deinit(__onedrive_curl_t* u)
{
	///
	if(u->opt_hdr)curl_slist_free_all(u->opt_hdr);
}

static size_t __onedrive_common_write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	__onedrive_curl_t* u = (__onedrive_curl_t*)stream;
	////
	u->last_active_time = time(0); ///

	u->result.append((char*)ptr, size*nmemb); ///
	return size*nmemb;
}

static void __combine_url_path(const char* rpath, int is_utf8_path, const char* attr, char* url)
{
	const char* ptr = rpath; while (*ptr == '/')++ptr;
	const char* sep = ":";
	if (*ptr == 0)sep = ""; //根目录
	sprintf(url, "https://graph.microsoft.com/v1.0/me/drive/root%s", sep); 
	__onedrive_conv_path(is_utf8_path, rpath, url + strlen(url));
	if(attr&& attr[0]) sprintf(url + strlen(url), "%s/%s", sep, attr);
}

static int __onedrive_common_request(__onedrive_t* h, const char* method, 
	const char* attr, const char* up_data, const char* rpath, cJSON** p_json, int* p_retcode)
{
	///
	char url_path[30 * 1024];
	__combine_url_path(rpath, h->is_utf8_path, attr, url_path);
	///
	__onedrive_curl_t u; int retry_auth_count = 0;
L:
	if (__onedrive_curl_init(&u, h, method, NULL, url_path) < 0) {
		return -1;
	}
	////
	u.opt_hdr = curl_slist_append(u.opt_hdr, "Content-type: application/json"); /// add Content-Type

	///
	curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_common_write_callback);
	curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);

	char ctx_len[128];
	if (up_data) {
		///
		curl_easy_setopt(u.curl, CURLOPT_POSTFIELDS, up_data);
		curl_easy_setopt(u.curl, CURLOPT_POSTFIELDSIZE, strlen(up_data)); ///
	}

	int ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////

	if (ret != CURLE_OK) {
		__onedrive_curl_deinit(&u);
		return -1;
	}

	int res_code = 0;
	curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);

	if (res_code == 401 || res_code == 403) {
		__onedrive_curl_deinit(&u);
		printf("__onedrive_common_request: not auth.\n");

		if (retry_auth_count < 1 && h->refresh_token_func) {
			retry_auth_count++ ;
			char token[64 * 1024]; 
			printf("__onedrive_common_request : in it Refresh Token. \n");
			ret = h->refresh_token_func(h, token, h->param);
			if (ret == 0) {// success get token .
				h->access_token = token;
				h->http_token = string("Authorization: Bearer ") + token; ///
				/////
				goto L;
			}
			//////
		}
		////
		return -1;
	}

	*p_retcode = res_code;
	////
	int r = 0;
//	printf("[%s]\n", u.result.c_str() );
	cJSON* json = cJSON_Parse(u.result.c_str());
	if (!json) {
		r = -2; //非网络连接错误
	}
	*p_json = json;
	/////
	__onedrive_curl_deinit(&u);

	return r;
}

static int64_t __onedrive_parse_mtime(const char* strtime)
{
	const char* s = strtime;
	int year = atoi(s);
	while (*s && *s != ' ' && *s != '-')s++;
	while (*s == ' ' || *s == '-')++s;
	int mon = atoi(s);
	while (*s && *s != ' ' && *s != '-')s++;
	while (*s == ' ' || *s == '-')++s;
	int day = atoi(s);
	////
	while (*s && *s != ' ' && *s != '-' &&*s!='T')s++;
	while (*s == ' ' || *s == '-' || *s=='T')++s;
	int h = atoi(s);
	while (*s && *s != ' ' && *s != ':')s++;
	while (*s == ' ' || *s == ':')++s;
	int m = atoi(s);
	while (*s && *s != ' ' && *s != ':')s++;
	while (*s == ' ' || *s == ':')++s;
	int sec = atoi(s);
	/////
	struct tm tt; memset(&tt, 0, sizeof(tt));
	tt.tm_year = (year >1900) ? (year - 1900) : year; tt.tm_mon = mon - 1; tt.tm_mday = day;
	tt.tm_hour = h; tt.tm_min = m; tt.tm_sec = sec;
	time_t gmt = mktime(&tt);

	long diff = 0; //获取区域时差
#ifdef WIN32
	TIME_ZONE_INFORMATION tz; memset(&tz, 0, sizeof(tz));
	GetTimeZoneInformation(&tz);
	diff = tz.Bias * 60; 
#else
	diff = 0; //这里没处理
#endif
	/////
	return gmt - diff; //
}
int onedrive_find_open(void* handle, const char* rpath, const char* findname)
{
	__onedrive_t* ctx = (__onedrive_t*)handle;
	if (!ctx)return -1;
	//////
	ctx->find_files.clear();
	ctx->find_iter = ctx->find_files.end();

	///
	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	int r = __onedrive_common_request(ctx, "GET", "children", NULL, rpath, &json, &status_code );
	if (r < 0) {
		return r;
	}

	int ret = 0; ////
	while (json) {
		////
		string nextLink;
		item = cJSON_GetObjectItem(json, "@odata.nextLink");
		if (item && item->type == cJSON_String) {
			nextLink = item->valuestring;
		}
		if (!nextLink.empty())printf("NextLink=[%s]\n", nextLink.c_str());

		////
		item = cJSON_GetObjectItem(json, "value");
		if (item == NULL || item->type != cJSON_Array) {
			cJSON_Delete(json);
			ret = -2;
			break;
		}
		cJSON* array = item;
		int cnt = cJSON_GetArraySize(array);
		for (int i = 0; i < cnt; ++i) {
			///
			cJSON* o = cJSON_GetArrayItem(array, i);
			item = cJSON_GetObjectItem(o, "name");if (item == NULL || item->type != cJSON_String) continue;
			char* name = item->valuestring;
			char name_buf[512] = "";
			if (!ctx->is_utf8_path) {
#ifdef WIN32
				wchar_t wp[1024] = L"";
				::MultiByteToWideChar(CP_UTF8, 0, name, -1, wp, sizeof(wp) / 2);
				::WideCharToMultiByte(CP_ACP, 0, wp, -1, name_buf, sizeof(name_buf), NULL, NULL);
				name = name_buf;
#endif
			}
			if (findname && strlen(findname) > 0) {
#ifdef WIN32
				if (!PathMatchSpec(name, findname)) { // Not Match
					continue; ///
				}
#else
				if (0 != fnmatch(findname, name, 0 )) {
					continue; ////
				}
#endif
			}

			////
			onedrive_name_t one; memset( &one, 0, sizeof(one));
			////
			item = cJSON_GetObjectItem(o, "size"); if (!item || item->type != cJSON_Number)continue;
			one.size = item->valueint;

			item = cJSON_GetObjectItem(o, "folder"); // is dir 
			if (item) {
				one.is_dir = true; ///
			}
			else {
				item = cJSON_GetObjectItem(o, "file"); if (!item) continue; /// must error.
				one.is_dir = false; /// file
			}
			
			item = cJSON_GetObjectItem(o, "createdDateTime");
			if (item && item->type == cJSON_String) {
				one.ctime = __onedrive_parse_mtime(item->valuestring);
			}
			item = cJSON_GetObjectItem(o, "lastModifiedDateTime");
			if (item && item->type == cJSON_String) {
			//	printf("[%s]\n", item->valuestring);
				one.mtime = __onedrive_parse_mtime(item->valuestring);
			}
			one.attr = one.is_dir ? FILE_ATTRIBUTE_DIRECTORY : 0;
			strcpy(one.name, name);  ///
	//		printf("[%s] size=%lld, is_dir=%d\n", one.name, one.size,one.is_dir );
			ctx->find_files.push_back(one); ///
		}

		//////
		cJSON_Delete(json); 
		json = NULL;
		if (nextLink.empty())break; //是否还有下一页
		
		/////
		__onedrive_curl_t u;
		if (__onedrive_curl_init(&u, ctx, "GET", NULL, nextLink.c_str() ) < 0) {
			ret = -1;
			break;
		}
		u.opt_hdr = curl_slist_append(u.opt_hdr, "Content-type: application/json"); ////

		curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_common_write_callback);
		curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);
		////
//		DWORD t = GetTickCount();
		ret = curl_run_timeout(ctx->mcurl, u.curl, &u.last_active_time, ctx->timeout); ////
//		printf("nextLink Request tm=%d\n", GetTickCount()-t);
		if (ret != CURLE_OK) {
			__onedrive_curl_deinit(&u);
			printf("onedrive_find_open: curl_run_timeout err\n");
			ret = -1;
			break;
		}
		//
		int res_code = 0;
		curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);
		json = cJSON_Parse(u.result.c_str());
		__onedrive_curl_deinit(&u);
		/////
		if (!json) {
			ret = -2;
			break;
		}
		///////
	}

	if (ret < 0) {
		ctx->find_files.clear();
		ctx->find_iter = ctx->find_files.end();   //
	}
	else {
		ctx->find_iter = ctx->find_files.begin(); //
	}
	////

	return ret;
}

/// -1错误，0 浏览完，1找到
int onedrive_find_next(void* handle, onedrive_name_t* name)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	/////
	if (h->find_iter == h->find_files.end()) return 0;

	*name = *h->find_iter;
	++h->find_iter;
	return 1;
}

int onedrive_stat(void* handle, const char* rpath, onedrive_stat_t* st)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	////
	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	int r = __onedrive_common_request(h, "GET", NULL, NULL, rpath, &json, &status_code);
	if (r < 0) {
		return r;
	}

	/////
	item = cJSON_GetObjectItem(json, "size");
	if (!item || item->type != cJSON_Number) {
		cJSON_Delete(json);
		return -2;
	}
	st->size = item->valueint;

	item = cJSON_GetObjectItem(json, "folder"); // is dir 
	if (item) {
		st->is_dir = true; ///
	}
	else {
		item = cJSON_GetObjectItem(json, "file"); 
		if (!item) {
			cJSON_Delete(json);
			return -2;
		}
		st->is_dir = false; /// file
	}

	item = cJSON_GetObjectItem(json, "createdDateTime");
	if (item && item->type == cJSON_String) {
		st->ctime = __onedrive_parse_mtime(item->valuestring);
	}
	item = cJSON_GetObjectItem(json, "lastModifiedDateTime");
	if (item && item->type == cJSON_String) {
		st->mtime = __onedrive_parse_mtime(item->valuestring);
	}
	st->attr = st->is_dir ? FILE_ATTRIBUTE_DIRECTORY : 0;

//	printf("%s\n", cJSON_Print(json));
	cJSON_Delete(json);

	return 0;
}

int onedrive_set_basicinfo(void* handle, const char* rpath,
	int attr, time_t atime, time_t mtime, time_t ctime)
{
	////
	return 0;
}

static int split_rpath(const char* rpath,bool is_utf8_path,  string& Path, string& Name)
{
	const char* name = strrchr(rpath, '/');
	if (!name)return -2;
	name++;
	string tmp_buf;
	if (*name == '\0') {
		--name;
		while (name >= rpath && *name == '/')--name;
		const char* end = name + 1;
		while (name >= rpath &&*name != '/')--name;
		++name;
		Path = string(rpath, name - rpath);
		tmp_buf = string(name, end - name);
		name = tmp_buf.c_str();
	}
	else {
		Path = string(rpath, name - rpath);
	}
	char conv_buf[4096];
	if (!is_utf8_path) {
#ifdef WIN32
		wchar_t wp[4096] = L"";
		::MultiByteToWideChar(CP_ACP, 0, name, -1, wp, sizeof(wp) / 2);
		::WideCharToMultiByte(CP_UTF8, 0, wp, -1, conv_buf, sizeof(conv_buf), NULL, NULL);
		Name = conv_buf;
#endif
		////
		
	}
	else {
		Name = name;
	}
	////
	return 0;
}
int onedrive_mkdir(void*handle, const char* rpath)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	///
	string name, path;
	if (split_rpath(rpath, h->is_utf8_path, path, name) < 0) {
		return -2;
	}
	////
	string req = string("{ \"name\":\"") + name +"\", \"folder\": { } }";

//	MessageBox(0, req.c_str(), path.c_str(), 0);

	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	int r = __onedrive_common_request(h, "POST", "children", req.c_str(), path.c_str(), &json, &status_code);
	if (r < 0) {
		return r;
	}

//	printf("code=%d, %s\n", status_code, cJSON_Print(json));
	cJSON_Delete(json);
	////
	if(status_code==201)
		return 0;

	return -2;
}

int onedrive_delfile(void* handle, const char* rpath)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	////
	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	int r = __onedrive_common_request(h, "DELETE", NULL, NULL, rpath, &json, &status_code);
	
	/////
	if (status_code == 204)
		return 0;

	return -2;
}

int onedrive_rename(void* handle, const char* oldpath, const char* newpath)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	////
	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;

	string new_name, new_path;
	if (split_rpath(newpath, h->is_utf8_path, new_path, new_name) < 0) {
		return -2;
	}
	/////
	if (!h->is_utf8_path) {
#ifdef WIN32
		wchar_t wp[1024]; char conv_buf[1024];
		::MultiByteToWideChar(CP_ACP, 0, new_path.c_str(), -1, wp, sizeof(wp) / 2);
		::WideCharToMultiByte(CP_UTF8, 0, wp, -1, conv_buf, sizeof(conv_buf), NULL, NULL);
		new_path = conv_buf;
#endif
	}

	string req = string("{ \"parentReference\": {\"path\":\"/drive/root:") + new_path + "\"}, \"name\":\"" + new_name + "\"}";
	
	int r = __onedrive_common_request(h, "PATCH", NULL, req.c_str(), oldpath, &json, &status_code);
	if (r < 0) {
		return r;
	}

//	printf("code=%d, %s\n", status_code, cJSON_Print(json));
	cJSON_Delete(json);

	if (status_code == 200)
		return 0;

	return -2;
}

/// download
static size_t __onedrive_offset_write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	__onedrive_curl_t* u = (__onedrive_curl_t*)stream;
	////
	u->last_active_time = time(0); ///
	size_t ll = size*nmemb;

	if (u->rd_off < u->rd_len) {
		////
		int len = min( (int)ll, u->rd_len - u->rd_off);

		memcpy(u->rd_buf + u->rd_off, ptr, len); ////
		u->rd_off += len;

		/////
		if (u->progress) {
			int r = u->progress(u->rd_off, u->rd_len, u->param); /////
			if (r < 0) { 
				printf("**** onedrive_offset_read: abort transfer...\n");
				////
				return 0; /////返回0， 表示人为的取消传输，这个时候 curl会关闭连接
			}
			//////
		}
	}
//	printf("read : len=%d\n", ll );
	return ll;
}
static int __onedrive_get_downloadUrl(__onedrive_t* h, bool* p_is_force, 
	const char* rpath, string& downloadUrl, int64_t* p_size)
{
	/// find from cache 
	if (!*p_is_force) {
		bool find = false;
		__onedrive_lock(h);
		map<string, __od_down_url>::iterator it = h->down_urls_cache.find(rpath);
		if (it != h->down_urls_cache.end()) {
			find = true;
			downloadUrl = it->second.url;
			*p_size = it->second.size;
		}
		__onedrive_unlock(h);
		///
		if (find) {
			return 0;
		}
	}

	//////
	*p_is_force = true; ///

	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	int r = __onedrive_common_request(h, "GET", NULL, NULL, rpath, &json, &status_code);
	if (r < 0) {
		return r;
	}

	/////
	item = cJSON_GetObjectItem(json, "size");
	if (!item || item->type != cJSON_Number) {
		cJSON_Delete(json);
		return -2;
	}
	*p_size = item->valueint;
	/////
	item = cJSON_GetObjectItem(json, "@microsoft.graph.downloadUrl");
	if (!item || item->type != cJSON_String) { //直接获取
		cJSON_Delete(json);
		///
		__onedrive_curl_t u;
		char url_path[30 * 1024];
		__combine_url_path(rpath, h->is_utf8_path, "content", url_path);
		///
		if (__onedrive_curl_init(&u, h, "GET", NULL, url_path) < 0) {
			return -1;
		}

		u.opt_hdr = curl_slist_append(u.opt_hdr, "Content-type: application/json"); /// add Content-Type
		///
		curl_easy_setopt(u.curl, CURLOPT_FOLLOWLOCATION, 0L); ////取消 302,301自动跳转

		int ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////

		if (ret != CURLE_OK) {
			__onedrive_curl_deinit(&u);
			return -1;
		}

		int res_code = 0;
		curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);
		if (res_code / 100 != 3) { //不是重定向
			__onedrive_curl_deinit(&u);
			printf("onedrive_offset_read : not 302 ");
			return -2;
		}
		char* file_url = NULL;
		ret = curl_easy_getinfo(u.curl, CURLINFO_REDIRECT_URL, &file_url);
		if (!file_url) {
			__onedrive_curl_deinit(&u);
			return -2;
		}
		/////
		downloadUrl = file_url;
		__onedrive_curl_deinit(&u);
		////
	}
	else {
		downloadUrl = item->valuestring;
		cJSON_Delete(json);
	}
//	printf("refresh down URL: [%s] -> [%s]\n ",rpath, downloadUrl.c_str());
	/// save to cache
	__onedrive_lock(h);
	__od_down_url url; url.url = downloadUrl; url.size = *p_size;
	h->down_urls_cache[rpath] = url;
	__onedrive_unlock(h);

	/////
	return 0;
}
int onedrive_offset_read(void* handle, const char* rpath,
	char* buffer, int64_t offset, int length,
	int(*progress)(int curr_trans, int total_len, void* pram), void* param)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	////
	__onedrive_curl_t u;
	string url; int64_t size = 0;

	bool is_force_flush = false;
	////

L:
	int ret = __onedrive_get_downloadUrl(h, &is_force_flush, rpath, url, &size);
	if (ret < 0) {
		return ret;
	}
	
	//
	if (offset >= size) { //请求超过界
		return 0; 
	}

	if (offset + length >= size) {
		length = size - offset; ///
	}
	/////
	char opt_hdr[256] = "";
	sprintf(opt_hdr, "Range: bytes=%lld-%lld", offset, offset + length - 1); /// pos [500-999], start 500, end 999 total 500

	if (__onedrive_curl_init(&u, h, "GET", opt_hdr, url.c_str(), false) < 0) {
		return -1;
	}
	//////
	u.rd_buf = buffer;
	u.rd_len = length;
	u.rd_off = 0;
	u.progress = progress;
	u.param = param;
	curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_offset_write_callback);
	curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);
//	DWORD t = GetTickCount();
	ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////
//	printf("Read Offset ms=%d\n",GetTickCount()-t);
	if (ret != CURLE_OK) {
		__onedrive_curl_deinit(&u);
		return -1;
	}

	int res_code = 0;
	curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);

	__onedrive_curl_deinit(&u);
	
	//////
	if (res_code / 100 != 2) {
		if (!is_force_flush) {
			is_force_flush = true;
			goto L;
		}
		////
		return -2;
	}

	return u.rd_off;
}


/// upfile
static size_t __upfile_read_callback(void *ptr, size_t size, size_t nitems, void *userdata)
{
	__onedrive_curl_t* u = (__onedrive_curl_t*)userdata;
	////
	int file_fd = (int)(long )u->rd_buf;
	u->last_active_time = time(0);
	int len = size*nitems;
	int left = u->rd_len - u->rd_off;
	if (left < len) {
		len = left;
	}
	if (!len)return 0;

	len = read(file_fd, ptr, len);

	if (u->progress2) {
		int64_t off = tell64(file_fd);
		int r = u->progress2(off, u->file_size, u->param); ///
		if (r < 0) {  // abort 
			printf("*** onedrive_upfile: transfer abort getdatalen=%d....\n", off);
			////
			u->is_abort_trans = true; 

			return CURL_READFUNC_ABORT; //返回CURL_READFUNC_ABORT， 表示人为的取消传输，这个时候 curl会关闭连接
		}
	}
	///
	u->rd_off += len;

	return len;
}

//严重错误-1， 否则 -2
static int __onedrive_session_upfile(__onedrive_t* h, int file_fd, 
	int64_t file_size, int64_t* p_offset, const char* url ,
	int(*progress)(int64_t cur_size, int64_t total_size, void* param), void* param)
{
	int BLOCK_SIZE = 10*320 * 1024;// 10 * 1024 * 1024; /// 10M 
	/////

	__onedrive_curl_t u;
	cJSON* json, *item;
	
	int64_t offset = *p_offset; ////
	////////
	if (lseek64(file_fd, offset, SEEK_SET) < 0) {
		printf("seek64 err \n");
		return -1;
	}

	////
	if (__onedrive_curl_init(&u, h, "PUT", NULL, url, false) < 0) {
		///
		return -1;
	}
	int len = min(BLOCK_SIZE, int(file_size - offset));
	////
	curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_common_write_callback);
	curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);
	curl_easy_setopt(u.curl, CURLOPT_READFUNCTION, __upfile_read_callback);
	curl_easy_setopt(u.curl, CURLOPT_READDATA, &u);
	curl_easy_setopt(u.curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(u.curl, CURLOPT_INFILESIZE, len);
	///
	
	char ctx_len[128]; char ctx_range[128];
	sprintf(ctx_len, "Content-Length: %d", len);
	sprintf(ctx_range, "Content-Range: bytes %lld-%lld/%lld", offset, offset + len - 1, file_size );
	u.opt_hdr = curl_slist_append(u.opt_hdr, ctx_len);
	u.opt_hdr = curl_slist_append(u.opt_hdr, ctx_range);
	u.opt_hdr = curl_slist_append(u.opt_hdr, "Expect:");

	u.file_size = file_size;
	u.rd_buf = (char*)(long)file_fd;
	u.rd_off = 0;
	u.rd_len = len;
	u.is_abort_trans = false;
	u.progress2 = progress;
	u.param = param;

	int ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////
	if (ret != CURLE_OK) {
		__onedrive_curl_deinit(&u);
		if (ret == CURLE_ABORTED_BY_CALLBACK) return -1; ////
		////
		return -2;
	}
	if (u.is_abort_trans) {
		printf("Trans ABort...\n");
		return -1;
	}

	int res_code = 0;
	curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);

	if (res_code == 201 || res_code == 200) { // success
		return 1;
	}
	else if (res_code == 202) { // continue
		///
		json = cJSON_Parse(u.result.c_str());
		if (!json)return -1;

		cJSON* array = cJSON_GetObjectItem(json, "nextExpectedRanges");
		if (!array || array->type != cJSON_Array) {
			cJSON_Delete(json);
			return -1;
		}
		item = cJSON_GetArrayItem(array, 0); //获取第一个 range
		if (!item) {
			cJSON_Delete(json);
			return -1;
		}
		int64_t offset = atoll(item->valuestring);
		cJSON_Delete(json);
		if (offset >= file_size) {
			return 1; /// success
		}

		*p_offset = offset; ///
		////
		return 0; 
	}
	else if (res_code == 416) { // request range 
		///
		if (__onedrive_curl_init(&u, h, "GET", NULL, url, false) < 0) {
			///
			return -1;
		}
		curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_common_write_callback);
		curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);

		int ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////
		if (ret != CURLE_OK) {
			__onedrive_curl_deinit(&u);
			return -2;
		}
		int res_code = 0;
		curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);
		__onedrive_curl_deinit(&u);
		if (res_code / 100 != 2) {
			if (res_code / 100 == 4)return -1;
			///
			return -2;
		}

		json = cJSON_Parse(u.result.c_str());
		if (!json) {
			return -1;
		}
		cJSON* array = cJSON_GetObjectItem(json, "nextExpectedRanges");
		if (!array || array->type != cJSON_Array) {
			cJSON_Delete(json);
			return -1;
		}
		item = cJSON_GetArrayItem(array, 0); //获取第一个 range
		if (!item) {
			cJSON_Delete(json);
			return -1;
		}
		int64_t offset = atoll(item->valuestring);
		cJSON_Delete(json);
		if (offset >= file_size) {
			return 1; /// success
		}

		*p_offset = offset; ///
		/////
	}

	/////
	return -2;
}

int onedrive_upfile(void* handle, const char* rpath, const char* lpath, int timeout,
	int(*progress)(int64_t cur_size, int64_t total_size, void* param), void* param)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	//////
	string p_path, name; 
	if (split_rpath(rpath, h->is_utf8_path, p_path, name) < 0) {
		return -2;
	}
	///
#ifdef WIN32
	int file_fd = open(lpath, O_RDONLY | O_BINARY, _S_IREAD | _S_IWRITE); ////
#else
	int file_fd = open(lpath, O_RDONLY, 0777);
#endif
	if (file_fd < 0) {
		printf("can not open [%s], err=%d \n", lpath , errno );
		return -1;
	}
	if (lseek64(file_fd, 0, SEEK_END) < 0) { close(file_fd); return -1; }
	int64_t file_size = tell64(file_fd);
	if (lseek64(file_fd, 0, SEEK_SET) < 0) { close(file_fd); return -1; }
	/////
	if (file_size == 0) {
		close(file_fd);
		return onedrive_set_newsize(h, rpath, 0); ////
	}

	///
	string req = string("{ \"item\": {\"name\":\"") + name + "\"} }";

	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	int r = __onedrive_common_request(h, "POST", "createUploadSession", req.c_str(), rpath, &json, &status_code);
	if (r < 0) {
		close(file_fd);
		return r;
	}

	item = cJSON_GetObjectItem(json, "uploadUrl");
	if (!item || item->type != cJSON_String) {
		close(file_fd);
		return -2;
	}
	string uploadUrl = item->valuestring; //printf("code=%d, %s\n", status_code, uploadUrl.c_str());
	cJSON_Delete(json); json = 0; item = 0;

	r = -1;////
	int retry_count = 0;
	int64_t offset = 0;
	while (true) {
		////
		r = __onedrive_session_upfile(h, file_fd, file_size, &offset, uploadUrl.c_str(), progress, param);
		if (r < 0) { // fail
			if (r == -1)break;
			++retry_count; 
			if (retry_count > 20)break;
		}
		else if(r==0) { // upload continue
			retry_count = 0;
		}
		else if (r > 0) { // upload complete 
			r = 0; 
			break;
		}
	}

	close(file_fd);
	///
	if (r < 0) {
		__onedrive_curl_t u;
		if (__onedrive_curl_init(&u, h, "DELETE", NULL, uploadUrl.c_str(), true) < 0) {
			///
			return -1; //
		}
		curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_common_write_callback);
		curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);
		////
		int ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////

		int res_code = 0;
		curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);
		__onedrive_curl_deinit(&u);
		printf("onedrive_upfile error , delete upload Session. statusCode=%d\n", res_code );
		////
		return r; 
	}
	
	////
	return r;
}

int onedrive_set_newsize(void* handle, const char* rpath, int64_t newsize)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	//////
	if (newsize > 0) {
		printf("OneDrive: not Randomly Access. [%s] newsize=%lld\n", rpath, newsize);
		return 0;
	}

	/// 只创建一个长度是0的文件
	int status_code = 0;
	cJSON* item = NULL;
	cJSON* json = NULL;
	__onedrive_curl_t u;
	char url_path[30 * 1024];
	__combine_url_path(rpath, h->is_utf8_path, "content", url_path);
	///
	if (__onedrive_curl_init(&u, h, "PUT", NULL, url_path) < 0) {
		return -1;
	}

	///
	u.opt_hdr = curl_slist_append(u.opt_hdr, "Content-Type: text/plain");
	u.opt_hdr = curl_slist_append(u.opt_hdr, "Content-Length: 0"); ///

	curl_easy_setopt(u.curl, CURLOPT_WRITEFUNCTION, __onedrive_common_write_callback);
	curl_easy_setopt(u.curl, CURLOPT_WRITEDATA, &u);
	
	int ret = curl_run_timeout(h->mcurl, u.curl, &u.last_active_time, h->timeout); ////

	if (ret != CURLE_OK) {
		__onedrive_curl_deinit(&u);
		return -1;
	}

	int res_code = 0;
	curl_easy_getinfo(u.curl, CURLINFO_RESPONSE_CODE, &res_code);

	__onedrive_curl_deinit(&u);

	if (res_code == 401 || res_code == 403) {
		
		printf("__onedrive_common_request: not auth.\n");
		return -1;
	}
//	printf("[%s]\n", u.result.c_str());
	if (res_code / 100 != 2)
		return -2;

	////
	return 0;
}

int onedrive_ping(void* handle, int timeout)
{
	__onedrive_t* h = (__onedrive_t*)handle;
	if (!h)return -1;
	//////

	onedrive_stat_t st;
	int r = onedrive_stat(handle, "/", &st); ////
	if (r == -1) return -1;
	////
	return 0;
}


/////////

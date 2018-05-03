///// by fanxiushu 2018-04-10
/// 实现的是 OneDrive 的 Microsoft Graph API
/// 这是属于xFsRedir项目工程的一部分，导出的接口函数类似于操作系统的文件操作函数的形式

#ifdef WIN32

#include <atlbase.h>
#include <atlwin.h>
#include <WinSock2.h>
#include "web_base.h"
#include <atlstr.h>
#include <atltypes.h>
#include <io.h>

#else
////
#define closesocket(s) close(s)
#include <sys/socket.h>
#include <arpa/inet.h>

#include <netdb.h>

#include <netinet/in.h>



#endif

#include <stdio.h>
#include <fcntl.h>
#include "cJSON.h"
#include "curl_base.h"

/////
#include <string>
#include <map>
#include <list>
using namespace std;
#include "onedrive.h"


////这是创建xFsRedir应用生成的 
#define ONEDRIVE_CLIENTID  "b7ba9e14-2265-4dc0-9305-8636aaf155a3"   ////
#define ONEDRIVE_CLIENTPWD "0*)xndmebbHVLPLCQ8818[]"
/////

static size_t _onedrive_common_write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	string* u = (string*)stream;
	///
	u->append((char*)ptr, size*nmemb); ///
	return size*nmemb;
}
static int __onedrive_get_token(const char* post_ctx, string& access_token, string& refresh_token, int& expire)
{
	int timeout = 15; 
	////
	CURL* curl = NULL;

	curl = curl_easy_init();
	if (!curl) {
		return -1;
	}
	CURLM* mcurl = curl_multi_init();
	if (!mcurl) {
		curl_easy_cleanup(curl);
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0); //// second
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout); /// second
	///当 transfer speed 小于 2字节/秒 时 将会在 timeout 秒内 abort 这个 transfer .update download 
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 2L);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, timeout);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST"); ///
	curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);

//	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	///
	curl_slist* hdrs = NULL;

	string result;
	
	hdrs = curl_slist_append(hdrs, "Content-Type: application/x-www-form-urlencoded");
	hdrs = curl_slist_append(hdrs, "User-Agent: HTTP.RestAPI-Curl_Library-Fanxiushu-xFsRedir-UserAgent");
	hdrs = curl_slist_append(hdrs, "Accept-Language: zh-cn,zh;q=0.8,en-us;q=0.5,en;q=0.3");
	hdrs = curl_slist_append(hdrs, "Expect:");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _onedrive_common_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_ctx);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(post_ctx));
	curl_easy_setopt(curl, CURLOPT_URL, "https://login.microsoftonline.com/common/oauth2/v2.0/token");

	time_t last_active_time = time(0);

	int ret = -1;
	int res_code = 0;
	do {
		ret = curl_run_timeout(mcurl, curl, &last_active_time, timeout); ////
		if (ret != CURLE_OK) {
			///
			printf("curl_run_timeout err");
			ret = -1;
			break;
		}
		
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
		ret = 0;
	} while (false);

	curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);
	curl_multi_cleanup(mcurl);
	
	/////
	if (ret < 0) {
		return -1;
	}

	/////
	cJSON* item;
	cJSON* json = cJSON_Parse(result.c_str()); ////
	if (!json) {
		printf("parse json[%s] err, resCode=%d\n", result.c_str() ,res_code );
		return -2;
	}

	item = cJSON_GetObjectItem(json, "access_token");
	if (!item || item->type != cJSON_String) {
		cJSON_Delete(json);
		printf("Can not get access_token\n");
		return -2;
	}
	access_token = item->valuestring;

	item = cJSON_GetObjectItem(json, "refresh_token");
	if (!item || item->type != cJSON_String) {
		cJSON_Delete(json);
		printf("Can not get refresh_token\n");
		return -2;
	}
	refresh_token = item->valuestring;

	item = cJSON_GetObjectItem(json, "expires_in");
	if (item) {
		expire = item->valueint;
	}
	else {
		expire = 3600;
	}
	cJSON_Delete(json);
	///
	return 0;
}

int onedrive_fromecode_token(const char* code, string& access_token, string& refresh_token, int& expire)
{
	char ctx[16*1024];
	sprintf(ctx, "client_id=%s&client_secret=%s&redirect_uri=http://localhost&code=%s&grant_type=authorization_code", 
		ONEDRIVE_CLIENTID, ONEDRIVE_CLIENTPWD, code);

	int r = __onedrive_get_token(ctx, access_token, refresh_token, expire);

	return r;
}

int onedrive_refresh_token(const char* old_refresh_token, string& access_token, string& refresh_token, int& expire)
{
	char ctx[16*1024];
	sprintf(ctx, "client_id=%s&client_secret=%s&redirect_uri=http://localhost&refresh_token=%s&grant_type=refresh_token",
		ONEDRIVE_CLIENTID, ONEDRIVE_CLIENTPWD, old_refresh_token);

	int r = __onedrive_get_token(ctx, access_token, refresh_token, expire);

	return r;
}

#ifdef WIN32
//////
class OneDriveCWebBrowser :public CWebBrowser
{
public:
	///
	virtual void on_navigate_before2(IDispatch* pDisp, VARIANT* URL,
		VARIANT* Flags, VARIANT* TargetFrameName,
		VARIANT* PostData, VARIANT* Headers, VARIANT_BOOL* Cancel)
	{
		_bstr_t url = _bstr_t(URL);
		const char* str_url = (const char*)url; const char* tok = "access_token="; printf("befor: %s\n", str_url);
		///
		const static char* code_str = "http://localhost/?code="; static int code_len = strlen(code_str);
		if (strnicmp(str_url, code_str, code_len) == 0) { ///
		//	*Cancel = TRUE;
		    ///
			const char* code = str_url + code_len;
		//	MessageBox(code, "", 0);
			expire_in = 3600;
			int r = onedrive_fromecode_token(code, access_token, refresh_token, expire_in);
			if (r < 0) {
				access_token = ""; 
				refresh_token = "";
				//
			}
			else {
	//			MessageBox(access_token.c_str()); MessageBox(refresh_token.c_str());
				printf("access_token=\n%s\n\nrefresh_token=\n%s\n", access_token.c_str(), refresh_token.c_str());
			}
			/////
			::DestroyWindow(::GetParent(m_hWnd));
		}
	//	MessageBox(str_url);
	}
	
	string access_token;
	string refresh_token;
	int    expire_in;
};
class COneDrive : public CWindowImpl < COneDrive >
{
public:
	DECLARE_WND_CLASS("OneDrive-OAuth2.0")

	BEGIN_MSG_MAP(COneDrive)
		////
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		////放到底部
		REFLECT_NOTIFICATIONS() //反射消息到子控件窗口
	END_MSG_MAP()
	/////
	string  url;
	OneDriveCWebBrowser web;
	/////
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		PostQuitMessage(0);
		return 1;
	}
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CRect rc; GetWindowRect(&rc);
		int cx = GetSystemMetrics(SM_CXSCREEN);
		int cy = GetSystemMetrics(SM_CYSCREEN);
		HWND parent = ::GetParent(m_hWnd);
		if (!parent) parent = GetActiveWindow();
		if (!parent) {
			int x = 0 + (cx - rc.Width()) / 2;
			int y = 0 + (cy - rc.Height()) / 2;
			SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
		else {
			//////
			CRect p_rc; ::GetWindowRect(parent, &p_rc);
			int x = p_rc.left + (p_rc.Width() - rc.Width()) / 2; if (x < 0)x = 0;
			int y = p_rc.top + (p_rc.Height() - rc.Height()) / 2; if (y < 0)y = 0;
			SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		rc = { 0,0,100,100 };
		HWND hwnd = web.Create(m_hWnd, rc, "", WS_CHILD | WS_VISIBLE);
		if (hwnd && web.m_pWebBrowser) {
			web.m_pWebBrowser->Navigate(CComBSTR(this->url.c_str()), NULL, NULL, NULL, NULL);
		}
		///
		return 1;
	}

	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CRect rc; GetClientRect(&rc);
		int dX = 10; int dY = 10;
		CRect web_rc = { dX, dY, rc.Width() - dX, rc.Height() - dX };
		web.MoveWindow(&web_rc);
		
		////
		return 1;
	}
	//////
};

static void atl_init(HINSTANCE h, BOOL bInit)
{
	static CComModule   _Module;
	static bool is_init = false;
	////
	if (bInit && !is_init) {
		_Module.Init(NULL, h);
		AtlAxWinInit();
		/////
		is_init = true;
	}
	else if (!bInit) {
	}
}
int onedrive_authorize(string& access_token, string& refresh_token)
{
	///
	string url;
	///
	url  = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=";
	url += ONEDRIVE_CLIENTID;
	url += "&scope=offline_access%20files.readwrite&response_type=code&redirect_uri=";
	url += "http://localhost";

	atl_init(GetModuleHandle(0), TRUE);
	/////
	COneDrive* one = new COneDrive;

	CRect rc = { 0,0, 1024, 600 };

	one->url = url;  //"http://www.baidu.com"; 
	one->Create(NULL, rc, "OneDrive-OAuth2.0", WS_OVERLAPPEDWINDOW | WS_SYSMENU | WS_MINIMIZEBOX); ///
	one->UpdateWindow();
	one->ShowWindow(SW_SHOW);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)>0) {
		////
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	access_token = one->web.access_token;
	refresh_token = one->web.refresh_token;

	/////
	delete one;
	////
	return 0;
}

#endif

///用浏览器方式打开

int onedrive_authorize_browser(string& access_token, string& refresh_token,
	    int timeout, int(*open_browser)(const char* url, void* param ), void* param  )
{
	
	string url;
	///
	url = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=";
	url += ONEDRIVE_CLIENTID;
	url += "&scope=offline_access%20files.readwrite&response_type=code&redirect_uri=";
	url += "http://localhost";

	////
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("socket err\n");
		return -1;
	}
	sockaddr_in addr; memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	unsigned int ip = inet_addr("127.0.0.1");
	memcpy(&addr.sin_addr, &ip, 4);
	addr.sin_port = htons(80);
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0 ) {
		closesocket(fd);
		printf("bind err=%d\n", errno);
		return -1;
	}
	listen(fd, 1);
	//////
	int r = open_browser(url.c_str(), param );
	if (r < 0) {
		closesocket(fd);
		return -1;
	}
	//////
	fd_set rdst; FD_ZERO(&rdst);
	FD_SET(fd, &rdst);
	timeval tmo; tmo.tv_usec = 0;
	tmo.tv_sec = timeout;
	r = select(fd + 1, &rdst, NULL, NULL, &tmo);
	if (r <= 0) {
		printf("Can not accept redirect URI.\n");
		closesocket(fd);
		return -1;
	}
	int s = accept(fd, NULL, NULL);
	closesocket(fd);
	if (s < 0) {
		printf("accept err\n");
		return -1;
	}
	/////accept data
	char buf[16 * 1024]; int pos = 0;
	r = -1;
	while (true) {
		r = recv(s, buf + pos, sizeof(buf) - pos, 0);
		if (r <= 0) {
			break;
		}
		pos += r;
		buf[pos] = 0;
		/////
		if (strstr(buf, "\r\n\r\n")) {
			break;
		}
	}
	if (r < 0) {
		closesocket(s);
		printf("recv error.\n");
		return -1;
	}
	////
	char* code_url = strstr(buf, "/?code="); 
	if (!code_url) {
		closesocket(s);
		printf("invalid request.\n");
		return -1;
	}
	code_url += strlen("/?code=");
	char* end = strstr(code_url, " "); if (end)*end = 0;
	printf("Code=%s\n", code_url);

	int expire = 00;
	r = onedrive_fromecode_token(code_url, access_token, refresh_token, expire);

	const char* body_ok = "<html><body><h2>Auth Complete<br>CLose the Page</body></html>";
	const char* body_err = "<html><body><h2>Auth Error<br>CLose the Page</body></html>";
	sprintf(buf, "%s\r\nContent-Length: %d\r\n"
		"\r\n%s",
		r< 0 ?"HTTP/1.0 403 err":"HTTP/1.0 200 OK", r<0?strlen(body_err):strlen(body_ok), 
		r<0 ?body_err:body_ok );

	send(s, buf, strlen(buf), 0);
	shutdown(s, 2);
	closesocket(s);
	////////
	return 0;
}

int onedrive_authorize_simple(
	int(*func)(const char* url, void* param1, void* param2 ), 
	void* param1, void* param2)
{
	string url;
	///
	url = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=";
	url += ONEDRIVE_CLIENTID;
	url += "&scope=offline_access%20files.readwrite&response_type=code&redirect_uri=";
	url += "http://localhost";
	////
	int r = func(url.c_str(), param1, param2);

	return r;
}

/////////////////////


///////////////
#if 0

static int browser_open(const char* url, void* param )
{
#ifdef WIN32
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOW); ///
#else
	return -1;
#endif
	/////
	return 0;
}

int main(int argc, char** argv)
{
	///
	string a_t, r_t; int expire;
	////
	WSADATA d; WSAStartup(0x0202, &d);
	curl_global_init(CURL_GLOBAL_DEFAULT);
	///
	
	onedrive_authorize_browser(a_t, r_t, 60, browser_open , NULL );

//	string ref_tok, acc_tok; 
//	onedrive_refresh_token(token, acc_tok, ref_tok, expire);
	printf("REFRESH=%s\n", a_t.c_str());
	///
	return 0;
}

#endif


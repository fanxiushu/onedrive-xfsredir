
///
#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
using namespace std;
#include "onedrive.h"

////
#if 1

static int browser_open(const char* url, void* param)
{
#if defined WIN32
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOW); ///

#elif defined __linux__
	char cmd[8192]; sprintf(cmd, "firefox \"%s\" &", url); // use firefox open url
	system(cmd);

#elif defined __APPLE__
	char cmd[8192]; sprintf(cmd, "open \"%s\"", url); // macos 
	system(cmd);

#else
#pragma error "not supported platform"

#endif
	/////
	return 0;
}


int refresh_token_func(void* handle, char* access_token, void* param)
{
	string acc_tok, ref_tok; int expire;
	///
//	onedrive_authorize(acc_tok, ref_tok);
//	strcpy(access_token, acc_tok.c_str());

//	int r = onedrive_refresh_token("", acc_tok,ref_tok,expire);
	printf("will Open Default browser \n");
	int r = onedrive_authorize_browser(acc_tok, ref_tok, 2 * 60, browser_open, NULL);
	if (r == 0) strcpy(access_token, acc_tok.c_str());
	else return -1;

	return 0;
}

static int read_offset_callback(int curr, int total, void* p)
{
	if (curr >= 1024 * 500) return -1; //will abort transfer
	return 0;
}
static int upfile_callback(int64_t curr, int64_t total, void* p)
{
	if (curr > 1024 * 768)return -1;// abort upfile
	return 0;
}
int main(int argc, char** argv)
{
//	curl_global_init(CURL_GLOBAL_DEFAULT);
	onedrive_name_t name;
	onedrive_stat_t st;
	int r;
//	string acc_tok, ref_tok; onedrive_authorize(acc_tok, ref_tok); printf("TOKEN=\n%s\n", acc_tok.c_str()); return 0;
	/////
	////
	void* h = onedrive_create("", 15, refresh_token_func, NULL);

	r = onedrive_find_open(h, "/", "*"); while (onedrive_find_next(h, &name) > 0) { printf("Name=%s, size=%lld, is_dir=%d, mtime=%lld\n", name.name,name.size,name.is_dir,name.mtime); }//printf("\n\nr=%d\n",r);
	/////
	r = onedrive_stat(h, "/xdisp_virt.exe", &st); if (r == 0)printf("stat: size=%lld, is_dir=%d,mtime=%lld\n", st.size,st.is_dir,st.mtime);
//	r = onedrive_mkdir(h, "/1/ff/"); printf("mkdir=%d\n", r); getchar();
//	r = onedrive_rename(h, "/1/ff", "/1/fan"); printf("rename=%d\n", r); getchar();
//	r = onedrive_delfile(h, "/1/fan"); printf("delfile=%d\n",r);

//	r = onedrive_set_newsize(h, "/1/tt.txt", 0); printf("set_newsize=%d\n", r);

	r = onedrive_upfile(h, "/test.log", "D:\\source\\xFsRedir\\export\\xfsredir.log", 15, upfile_callback, NULL); printf("upfile=%d\n", r );
//	getchar();
//	r = onedrive_upfile(h, "/test.log", "D:\\source\\xFsRedir\\export\\xfsredir.log", 15, NULL, NULL); printf("upfile=%d\n", r);

	static char buf[10 * 1024 * 100]; memset(buf, 0, sizeof(buf));
	r = onedrive_offset_read(h, "/xdisp_virt.exe", buf, 1024, 1024 * 1024 * 10, read_offset_callback, 0); printf("read=%d, \n", r );
	r = onedrive_stat(h, "/xdisp_virt.exe", &st); if (r == 0)printf("stat: size=%lld, is_dir=%d,mtime=%lld\n", st.size, st.is_dir, st.mtime);
	getchar();
}
#endif


//// By fanxiushu 2016
/// 实现的是 OneDrive 的 Microsoft Graph API
/// 这是属于xFsRedir项目工程的一部分，

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (defined _WIN32 || defined _WIN64)

#define msleep(m) Sleep(m)
// for PathMatchSpec 通配符比较
#include <Shlwapi.h>
#pragma comment(lib,"shlwapi")

#else

#define msleep(m) {struct timespec t;t.tv_sec=m/1000;t.tv_nsec=(m%1000)*1000000; nanosleep(&t,NULL);}
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>  
#include <sys/types.h> 
#include <dirent.h>

#ifndef O_BINARY
#define O_BINARY    0 
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>

#ifndef stat64
//#define stat64  stat
#define lseek64 lseek
#endif
#endif // __APPLE__

#endif // !WIN32

#include <string>
#include <list>
#include <vector>
#include <map>
using namespace std;

extern "C" {

#ifdef WIN32
#define CURL_STATICLIB
#include "../win32/inc/curl.h"
#else
#if defined __linux
#include <curl/curl.h>

#elif defined __APPLE__ // iOS
#include "../ios/libcurl-ios-dist/include/curl/curl.h"
#endif

#endif
}


/////
int curl_run_timeout(CURLM* mcurl, CURL* curl, time_t* last, int timeout);

int base64_encode(const unsigned char *in, unsigned long len,
	unsigned char *out);
int base64_decode(const unsigned char *in, unsigned char *out);


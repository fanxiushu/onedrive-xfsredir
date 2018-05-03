/////
/// 实现的是 OneDrive 的 Microsoft Graph API
/// 这是属于xFsRedir项目工程的一部分，

#include "curl_base.h"

#ifdef WIN32

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"wldap32.lib")
#pragma comment(lib,"Crypt32.lib")
/////
#pragma comment(lib,"./lib/libeay32.lib")
#pragma comment(lib,"./lib/ssleay32.lib")
#pragma comment(lib,"./lib/libssh2.lib")
#pragma comment(lib,"./lib/libcurl.lib")


/*
* 当libjpeg-turbo为vs2010编译时，vs2015下静态链接libjpeg-turbo会链接出错:找不到__iob_func,
* 增加__iob_func到__acrt_iob_func的转换函数解决此问题,
* 当libjpeg-turbo用vs2015编译时，不需要此补丁文件
*/
#if _MSC_VER>=1900
#include "stdio.h" 
_ACRTIMP_ALT FILE* __cdecl __acrt_iob_func(unsigned);
#ifdef __cplusplus 
extern "C"
#endif 
FILE* __cdecl __iob_func(unsigned i) {
	return __acrt_iob_func(i);
}
#endif /* _MSC_VER>=1900 */

#else ///

#endif

//////
int curl_run_timeout(CURLM* mcurl, CURL* curl, time_t* last, int timeout)
{
	int still_running = 1;
	int ret;

	////

	curl_multi_add_handle(mcurl, curl); ////

	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(mcurl, &still_running)) {
		///
		printf("curl_multi_perform return CURLM_CALL_MULTI_PERFORM 1\n");
	}

	ret = 0; 

	while (still_running > 0 ) {
		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;
		int maxfd = -1;
		int rc;
		CURLMcode mc;
		/////
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		mc = curl_multi_fdset(mcurl, &fdread, &fdwrite, &fdexcep, &maxfd);
		if (mc != CURLM_OK) {
			ret = -1;
			break;
		}

		struct timeval tmo_tv; 
		tmo_tv.tv_sec = 1;  //// 1 second
		tmo_tv.tv_usec = 0; 

		if (maxfd == -1) {
			rc = 0;// printf("Sleep 30\n");
			msleep(1); ///停留 30 ms
		}
		else {
			rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &tmo_tv);
		}
		if (rc < 0) {
			ret = -1;
			printf("--------------curl select error <%d>\n", errno );
			break;
		}
		if (abs(time(0) - *last) > timeout) {
			printf("---------****------ curl timeout .\n");
			ret = -1;
			break;
		}
		//////

		while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(mcurl, &still_running)) {
			///
			printf("-- curl_multi_perform return CURLM_CALL_MULTI_PERFORM 2\n");
		}
		////////

	}
	/////
	curl_multi_remove_handle(mcurl, curl);
	////
	return ret;
}

///////////////////base64
static const char *base64codes =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char base64map[256] = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 253, 255,
	255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 253, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
	52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
	255, 254, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,
	7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,
	19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
	255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
	37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
	49,  50,  51, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255 };

int base64_encode(const unsigned char *in, unsigned long len,
	unsigned char *out)
{
	unsigned long i, len2, leven;
	unsigned char *p;
	/* valid output size ? */
	len2 = 4 * ((len + 2) / 3);
	p = out;
	leven = 3 * (len / 3);
	for (i = 0; i < leven; i += 3) {
		*p++ = base64codes[in[0] >> 2];
		*p++ = base64codes[((in[0] & 3) << 4) + (in[1] >> 4)];
		*p++ = base64codes[((in[1] & 0xf) << 2) + (in[2] >> 6)];
		*p++ = base64codes[in[2] & 0x3f];
		in += 3;
	}
	/* Pad it if necessary...  */
	if (i < len) {
		unsigned a = in[0];
		unsigned b = (i + 1 < len) ? in[1] : 0;
		unsigned c = 0;

		*p++ = base64codes[a >> 2];
		*p++ = base64codes[((a & 3) << 4) + (b >> 4)];
		*p++ = (i + 1 < len) ? base64codes[((b & 0xf) << 2) + (c >> 6)] : '=';
		*p++ = '=';
	}

	/* append a NULL byte */
	*p = '\0';

	return p - out;
}

int base64_decode(const unsigned char *in, unsigned char *out)
{
	unsigned long t, x, y, z;
	unsigned char c;
	int g = 3;

	for (x = y = z = t = 0; in[x] != 0;) {
		c = base64map[in[x++]];
		if (c == 255) return -1;
		if (c == 253) continue;
		if (c == 254) { c = 0; g--; }
		t = (t << 6) | c;
		if (++y == 4) {
			//          if (z + g > *outlen) { return CRYPT_BUFFER_OVERFLOW; }  
			out[z++] = (unsigned char)((t >> 16) & 255);
			if (g > 1) out[z++] = (unsigned char)((t >> 8) & 255);
			if (g > 2) out[z++] = (unsigned char)(t & 255);
			y = t = 0;
		}
	}
	//  if (y != 0) {  
	//      return -1;  
	//  }  
	return z;
}

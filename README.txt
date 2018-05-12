源代码实现了OneDrive客户端接口函数集合，
导出的接口函数类似于操作系统的文件操作函数。
比如：
int onedrive_stat(void* handle, const char* rpath, onedrive_stat_t* st); 类似stat函数
int onedrive_mkdir(void*handle, const char* rpath); 类似mkdir函数
int onedrive_rename(void* handle, const char* oldpath, const char* newpath); 类似 rename函数
int onedrive_offset_read(void* handle, const char* rpath,
	char* buffer, int64_t offset, int length,
	int(*progress)(int curr_trans, int total_len, void* pram), void* param);类似read文件读函数。

更详细的内容请查阅 onedrive.h头文件。


-------------------------------------------------------
源代码属于xFsRedir项目工程的一部分。
xFsRedir是在windows平台下实现目录重定向，也就是把多个异构的网络文件系统集中到windows进行访问。
xFsRedir软件下载地址：
https://github.com/fanxiushu/xFsRedir

详细信息查阅：
https://blog.csdn.net/fanxiushu/article/details/80289261

-------------------------------------------------------
源代码支持多个平台编译，支持平台包括：
Windows，linux， MacOS，iOS.
Andriod平台暂时没做移植，有兴趣的话，可自行移植。

windows 使用VS2015编译，
linux， macOS打开终端，进入onedrive-xfsredir目录，make 即可编译生成onedrive-xfsredir实例程序。
iOS程序在macOS中打开Xcode来编译。当前编译的Xcode是 9.3 版本。

源代码需要使用libcurl开源库，windows和iOS已经编译成静态库。

fanxiushu 2018




#pragma once
#include <stdio.h>
#include <ctype.h>
int initListenFd(unsigned short port);

//启动epoll
int epollRun(int lfd);

//和客户端建立链接
int acceptClient(int lfd,int epfd);
// void* acceptClient(void* arg);

//接受http请求
int recvHttpRequest(int cfd,int epfd);
// void* recvHttpRequest(void* arg);
//解析请求行
int parseRequestLine(const char* line,int cfd);

//发送文件
int sendFile(char* fileName,int cfd);

//发送响应头（状态行 + 响应头）
int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length);
//通过文件后缀获取文件的content- type
const char* getFileType(const char* name);

//发送目录
int sendDir(char* dirName,int cfd);

//将字符转化为整形数
int hexToDec(char c);
//解码
//to 存储解码之后的数据，传出参数，from被解码的数据，传入参数
void decodeMsg(char* to,char* from);

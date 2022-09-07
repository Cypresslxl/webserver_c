#include "Server.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>

struct FdInfo
{
    int fd;          //通信文件描述符
    int epfd;        // epoll文件描述符
    pthread_t tid;   //线程id
};

int initListenFd(unsigned short port)
{
    //设置监听的file descriptor
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1)
    {
        perror("socket");
        return -1;
    }
    int opt = 1;
    //设置端口复用
    int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret==-1)
    {
        perror("setsockopt");
        return -1;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;      //ipv4 协议族
    addr.sin_port = htons(port);    //转化为大端（网络字节序）
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd,(struct sockaddr*)&addr,sizeof addr);
    if(ret==-1)
    {
        perror("bind");
        return -1;
    }
    ret = listen(lfd,128);  //内核最大128 ，设置大于128的数 内核也会把它恢复为128
    if(ret==-1)
    {
        perror("listen");
        return -1;
    }

    return lfd;
}

int epollRun(int lfd)
{
    //1.创建epoll 实例
    int epfd = epoll_create(1); //参数已被弃用，但是必须大于1
    if(epfd==-1)
    {
        perror("epoll_create");
        return -1;
    }

    //2.把用于监听的文件描述符添加到 epoll树上
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;    //读事件
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret==-1)
    {
        perror("epoll_create");
        return -1;
    }
    //3.检测
    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(epoll_event);
    while(true)
    {
        int num = epoll_wait(epfd,evs,size,-1);  // 委托内核帮助我们检测添加到epoll树（模型）上的文件描述符监听的事件是否被激活了
        for(int i = 0;i<num;++i)
        {
            int fd = evs[i].data.fd;
            struct FdInfo* info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
            info->epfd = epfd;
            info->fd = fd;

            if(fd==lfd) //如果是监听的文件描述符
            {   //建立新链接    accept
                acceptClient(lfd,epfd);
                // pthread_create(&info->tid,nullptr,acceptClient,&info);
            }
            else
            {  //用于通信的文件描述符，主要是接受对端的数据
                recvHttpRequest(fd,epfd);
                // pthread_create(&info->tid,nullptr,recvHttpRequest,&info);
            }
        }
    }
    return 0;
}
// void* acceptClient(void* arg)
int acceptClient(int lfd,int epfd)
{
    // struct FdInfo* info = (struct FdInfo*)arg;
    //1.建立链接
    int cfd = accept(lfd,nullptr,nullptr);  //通过监听的文件描述符lfd accept得到 通信文件描述符
    if(cfd==-1)
    {
        printf("this...\n");
        perror("accept");
        return 0;
    }
    //得到通信的文件（默认为阻塞） 设置为非阻塞，用fctl
    int flag = fcntl(cfd,F_GETFL);  //得到文件描述符的属性 getflag
    flag |= O_NONBLOCK;     //设置非阻塞
    fcntl(cfd,F_SETFL,flag);        //setflag 需要第三个参数

    //把文件描述符cfd添加的epoll 模型里
    //边沿非阻塞
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;  //events 设置读 和 ET模式
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);    //epoll树上添加监听的文件描述符以及监听的事件
    if(ret==-1)
    {
        perror("epoll_ctl");
        return -1;
    }
    // printf("acceptClient tid:%ld\n",info->tid);
    // free(info);
    return 0;
}


// void* recvHttpRequest(void* arg)
int recvHttpRequest(int cfd,int epfd)
{
    // struct FdInfo* info = (struct FdInfo*)arg;
    printf("recvHttpRequest开始接受数据...\n");
    char buf[4096] = { 0 };
    char temp[1024] = {0};
    int len = 0,total = 0;
    //由于是 ET 边沿模式，所以需要一次性全部取出来
    //通过recv 将客户端发来的数据进行保存
    while((len = recv(cfd,temp,sizeof temp,0))>0)
    {
        if((total+len) < sizeof(buf))
        {
            memcpy(buf + total,temp,len);
        }
        total += len;
    }
    //recv数据读完了或者出错了 都会return -1,通过errno判断
    //errno是linux 提供的全局变量，当函数调用失败后，errno就会指定成一个合适的值 ，通过errno的
    //值就可以查找errno 编号对应的错误信息
    //判断数据是否接收完毕
    if(len==-1 && errno == EAGAIN)
    {
        //解析http，解析请求行
        // printf("%s\n",buf);
// GET / HTTP/1.1
// Host: 10.211.55.3:10000
// Connection: keep-alive
// Cache-Control: max-age=0
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/104.0.0.0 Safari/537.36
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
// Accept-Encoding: gzip, deflate
// Accept-Language: zh-CN,zh;q=0.9
        char* pt = strstr(buf,"\r\n");    //在一个大的字符串搜索子字符串的函数
        int reqlen = pt - buf;  //得到请求行的长度
        buf[reqlen] = '\0'; //截断字符串
        // printf("%s\n",buf);
        // GET / HTTP/1.1
        printf("%s\n",buf);
        parseRequestLine(buf,cfd);
    }
    else if(len==0) //说明客户端已经断开了链接
    {
        //需要把通信的文件描述符从epoll 树上delete
        epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,nullptr);
        //释放文件描述符
        close(cfd);
    }
    else
    {
        perror("recv");
    }
    // printf("recvMsg tid:%ld\n",info->tid);
    // close(info->fd);    //关闭通信的文件描述符
    // free(info);
    return 0;
}

//

int parseRequestLine(const char* line,int cfd)
{
    //解析请求行
    printf("开始解析数据...\n");
    char method[12];    //get 或 post
    char path[1024];    //客服端请求的静态资源路径
    // printf("http request:%s\n",line);
    // GET / HTTP/1.1
    sscanf(line,"%[^ ] %[^ ]",method,path);//注意中间有空格
    // printf("method :%s\npath :%s\n",method,path);
    //判断是post请求还是 get请求
    if(strcasecmp(method,"get")!=0) //strcasecmp 判断不区分大小写，GET也是可以的，return 0 表示相等
    {
        perror("http request isn't GET\n");
        return -1;
    }
    //处理客服端请求的静态资源（目录或文件）
    decodeMsg(path,path);
    // printf("after decodeMsg :%s\n",path);
    char* file = nullptr;
    if(strcmp(path,"/")==0) //目录
    {
        file = "./";
    }
    else                    //文件
    {
        file = path + 1;
    }
    printf("file:%s\n",file);
    //获取文件的属性
    struct stat st;
    int ret = stat(file , &st);
    if(ret==-1)
    {   //fail
        //文件不存在    -- 回复 404
        sendHeadMsg(cfd,404,"Not Found",getFileType(".html"),st.st_size);
        sendFile("404.html",cfd);
        return 0;
    }
    // else 判断文件类型
    if(S_ISDIR(st.st_mode)) //linux提供的宏函数，判断文件属性是不是目录
    {
        //把目录的内容发送给客户端 
        // printf("目录\n");
        sendHeadMsg(cfd,200,"OK",getFileType(".html"),-1);
        sendDir(file,cfd);
    }
    else//文件 
    {
        //把文件的内容发生给客户端
        // printf("文件\n");
        sendHeadMsg(cfd,200,"OK",getFileType(file),st.st_size);   //-1表示不知道文件大小
        sendFile(file,cfd);
    }
    return 0;
}

int sendFile(char* fileName,int cfd)
{
    //1.打开文件
    int fd = open(fileName,O_RDONLY);
    printf("sendFile...filename :%s\n",fileName);
    assert(fd > 0);

#if 0
    while(true)
    {
        char buf[1024];
        int len = read(fd,buf,sizeof(buf));
        if(len > 0)
        {
            send(cfd,buf,len,0);
            usleep(10); //防止客户端缓冲爆满
        }else if(len==0)    //读完了
        {
            break;
        }
        else
        {   //读信息出错
            perror("read");
        }
    }
#else
    off_t offset = 0;
    int size = lseek(fd,0,SEEK_END);    //能获取文件的大小，但是文件指针移动到了尾部
    lseek(fd,0,SEEK_SET);   //把文件指针移动到文件头部
    while(offset<size)
    {
    //sendfile 第三个参数offset 1.发送数据之前根据该偏移量开始读文件数据 2.发送数据之后，更新偏移量 （不需要程序员管理这个偏移量）
        int ret = sendfile(cfd,fd,&offset,size - offset);    //linux 里的一个高级函数，号称零拷贝
        // printf("ret value : %d",ret);    //cfd 非阻塞
        // if(ret==-1)
        // {
        //     perror("sendfile\n");
        // }
    }
    
#endif 
    close(fd);  //关闭文件描述符是为了释放资源
    return 0;
}

const char* getFileType(const char* name)
{
    // a.jpg a.mp4 a.html
    //从左到右找 "."字符，未找到则返回 nullptr 
    const char* dot = strrchr(name,'.');
    if(dot == nullptr)
    {
        return "text/plain; charset=utf-8";//纯文本
    }
    if(strcmp(dot,".html")==0||strcmp(dot,".htm")==0)
        return "text/html; charset=utf-8";
    if(strcmp(dot,".jpg")==0)
        return "image/jpeg;";
    if(strcmp(dot,".gif")==0)
        return "image/gif";

    return "text/plain; charset=utf-8";//纯文本
}

int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length)
{
    // printf("sendHeadMsg\n");
    //状态行
    char buf[1024] = { 0 };
    sprintf(buf,"http/1.1 %d %s\r\n",status,descr);

    //响应头
    sprintf(buf + strlen(buf),"content-type: %s\r\n",type);
    sprintf(buf + strlen(buf),"content-length: %d\r\n\r\n",length);//第三部分空行，也就是\r\n

    send(cfd,buf,strlen(buf),0);
    return 0;
}
int sendDir(char* dirName,int cfd)
{
    // printf("sendDir,dirname:%s\n",dirName);
    char buf[4096] = { 0 };
    sprintf(buf,"<html><head><title>%s</title></head><body><table>",dirName);
    //function 通过调用这个函数获取某个目录里的文件的名字，再把文件的名字组织到一起发送给客户端
    struct dirent** namelist;
    //namelist 传出参数，保存遍历到的文件
    int num = scandir(dirName,&namelist,nullptr,alphasort);//返回文件的数量
    for(int i = 0;i<num;++i)
    {
        //取出文件名    namelist指向的是一个指针数组 struct dirent* tmp[]
        char* name = namelist[i]->d_name;
        struct stat st;
        char subPath[1024] = { 0 };
        sprintf(subPath,"%s/%s",dirName,name);  //将目录名和文件名进行拼接
        stat(subPath,&st); //通过stat函数得到文件的属性（目录或非目录）传出给 st
        if(S_ISDIR(st.st_mode))
        {//是目录
            //a标签<a href="">name</a>
            //第一个%s后的 \ 表示要跳转到的目录，因为是目录所以需要加 \  ,没有 \ 代表文件
            // printf("sendDir:是目录%s\n",subPath);
            sprintf(buf+strlen(buf),"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",name,name,st.st_size);
        }
        else
        {//不是目录
            // printf("sendDir:不是目录%s\n",subPath);
            sprintf(buf+strlen(buf),"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",name,name,st.st_size);

        }
        //拼接好html 字符串后就可以发送了
        // printf("%s",buf);
        send(cfd,buf,strlen(buf),0);
        memset(buf,0,sizeof(buf));
        free(namelist[i]);//释放指针
    }
    sprintf(buf,"</table></body></html>");
    // printf("%s\n",buf);
    send(cfd,buf,strlen(buf),0);
    free(namelist); //释放二级指针
    // printf("sendDir 结束\n");
    return 0;
}

int hexToDec(char c)
{
    if(c >= '0' && c<= '9')
        return c - '0';
    if(c >= 'a'&& c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

void decodeMsg(char* to,char* from)
{
    for(;*from != '\0';++to,++from)
    {
        //isxdigit -> 判断字符是不是16进制格式，取值在 0-f
        //linux
        if(from[0]=='%'&&isxdigit(from[1])&&isxdigit(from[2]))
        {


            *to = hexToDec(from[1])*16 + hexToDec(from[2]);
            from += 2;
        }
        else
        {
            *to = *from;
        }
    }
    *to = '\0';
}
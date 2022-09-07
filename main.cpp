#include "Server.h"
#include <stdlib.h>
#include <unistd.h>
 
int main(int argc,char* argv[])
{
    if(argc<3)
    {
        printf("./a.out port /home/parallels/Desktop/web/src \n");
        return -1;
    }

    unsigned short port = atoi(argv[1]); 

    //切换服务器工作路径
    chdir(argv[2] );

    int lfd = initListenFd(port);  //最好选择5000以后的端口号

    //启动服务器程序
    epollRun(lfd);
    return 0;
}
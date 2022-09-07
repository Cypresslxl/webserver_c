//
// Created by Cypress on 2022/6/27.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
using namespace std;

int main(int argc, char** argv)
{
        // 1. 建立一个监听的 socket fd
        int lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (lfd < 0)
        {
                perror("socket");
                exit(0);
        }

        // 2. 设置请求连接的server 地址
        sockaddr_in sock_in = {};    // sockaddr_in为网络地址的结构体
        sock_in.sin_family = AF_INET;
        int port = 10000;
        sock_in.sin_port = htons(port);

        string ip;
        cout << "please input your server ip: ";
        cin >> ip;

        if (inet_pton(AF_INET, server_ip.c_str(), &_sevrAddr.sin_addr) <= 0)
        {
                perror("inet_pton");
                exit(0);
        }

        // 3. 连接服务端
        int ret = connect(lfd, (struct sockaddr*)&sock_in, sizeof(sock_in));
        if (ret < 0)
        {
                perror("connect");
                exit(0);
        }
        else
        {
                cout << "connect to server: " << ip << ":" << port << " success!" << endl;
        }

        // 4. 向服务端发送数据
        while(true)
        {
            string sendData;
            cout << "please input send data: ";
            getchar();
            getline(cin, sendData);
            if(send(lfd, sendData.c_str(), sendData.length(), 0) < 0)
            {
                    perror("send");
                    exit(0);
            }
        }

        close(lfd);
        return 0;
}

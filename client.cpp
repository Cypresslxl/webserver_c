//
// Created by Evila on 2021/6/27.
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
        // 1. 建立一个Socket
        int _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sock < 0)
        {
                cout << "create socket error: " << strerror(errno) << "%s(errno: " << errno << endl;
                exit(0);
        }

        // 2. 设置请求连接的server 地址
        sockaddr_in _sevrAddr = {};    // sockaddr_in为网络地址的结构体
        _sevrAddr.sin_family = AF_INET;
        int port = 45678;
        _sevrAddr.sin_port = htons(port);

        string server_ip;
        cout << "please input server ip: ";
        cin >> server_ip;

        if (inet_pton(AF_INET, server_ip.c_str(), &_sevrAddr.sin_addr) <= 0)
        {
                cout <<"inet_pton error for " << server_ip << endl;
                exit(0);
        }

        // 3. 连接服务端
        int ret = connect(_sock, (struct sockaddr*)&_sevrAddr, sizeof(_sevrAddr));
        if (ret < 0)
        {
                cout << "connect error: " << strerror(errno) << "errno: " << errno << endl;
                exit(0);
        }
        else
        {
                cout << "connect to server: " << server_ip << ":" << port << " success!" << endl;
        }

        // 4. 向服务端发送数据
        while(true)
        {
            string sendData;
            cout << "please input send data: ";
            getchar();
            getline(cin, sendData);
            if(send(_sock, sendData.c_str(), sendData.length(), 0) < 0)
            {
                    cout << "send msg error: " << strerror(errno) << "errno: " << errno << endl;
                    exit(0);
            }
        }

        close(_sock);
        exit(0);
}

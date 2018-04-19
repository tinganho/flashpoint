//
// Created by Tingan Ho on 2018-04-15.
//

#ifndef FLASH_TCP_CLIENT_H
#define FLASH_TCP_CLIENT_H

#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace flashpoint::lib {
    class TcpClient {
    private:
        int socketfd;
        unsigned int port;
        hostent server;
        sockaddr_in server_address;
        const char* host;
    public:
        TcpClient(const char*, unsigned int port);
        bool _connect();
    };
}


#endif //FLASH_TCP_CLIENT_H

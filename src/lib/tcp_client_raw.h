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
class TcpClientRaw {
private:

    int
    socketfd;

    hostent
    server;

    sockaddr_in
    server_address;

public:

    TcpClientRaw();

    bool
    _connect(const char*, unsigned int port);

    void
    send_message(const char*);

    char*
    recieve_message();
};

}


#endif //FLASH_TCP_CLIENT_H

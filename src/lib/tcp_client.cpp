#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <lib/tcp_client.h>

namespace flashpoint::lib {

    TcpClient::TcpClient(const char* host, unsigned int port):
        socketfd(-1),
        port(port),
        host(host)
    { }

    bool TcpClient::_connect()
    {
        socketfd = socket(AF_INET, SOCK_STREAM, 0);
        char buffer[1024] = {0};
        const char *hello = "Hello from client";
        if (socketfd < 0) {
            throw std::domain_error("Could not open socket.");
        }
        memset(&server_address, '0', sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);

        if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
            throw std::domain_error("Invalid address.");
        }
        if (connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
            throw std::domain_error("Connection to socket failed.");
        }
        send(socketfd, hello, strlen(hello), 0);

        return true;
    }
}
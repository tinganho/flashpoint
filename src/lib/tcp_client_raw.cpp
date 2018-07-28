#include <stdexcept>
#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <lib/tcp_client_raw.h>

#define CHUNK_SIZE 1024

namespace flashpoint::lib {

TcpClientRaw::TcpClientRaw():
    socketfd(-1)
{ }

bool
TcpClientRaw::bind(const char *host, unsigned int port)
{
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0) {
        throw std::domain_error("Could not open socket.");
    }
    memset(&server_address, '0', sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
        throw std::domain_error("Invalid address.");
    }
    if (connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        throw std::domain_error("Connection to socket failed.");
    }

    return true;
}

void
TcpClientRaw::send_message(const char* message)
{
    if (send(socketfd, message, strlen(message), 0) < 0) {
        std::cerr << "Send failed : " << std::endl;
    }
}

char*
TcpClientRaw::recieve_message()
{
    char* chunk = new char[CHUNK_SIZE];
    ssize_t size_received = recv(socketfd, chunk, CHUNK_SIZE, 0);
    if (size_received < 0) {
        std::cout << "Receive failed"<< std::endl;
    }
    chunk[size_received] = '\0';
    return chunk;
}

}
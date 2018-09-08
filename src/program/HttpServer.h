#ifndef FLASHPOINT_HTTP_SERVER_H
#define FLASHPOINT_HTTP_SERVER_H

#include <uv.h>
#include <openssl/ssl.h>
#include <program/HttpParser.h>
#include <lib/memory_pool.h>
#include <glibmm/ustring.h>
#include <program/graphql/graphql_syntaxes.h>
#include <vector>

using namespace flashpoint::program::graphql;

namespace flashpoint {

class HttpServer {
public:
    HttpServer(uv_loop_t* loop);
    void Listen(const char *host, unsigned int port);
    void Listen(const char *host, unsigned int port, int fd);
    void Close();

    uv_loop_t* loop;
    SSL_CTX* ssl_ctx;
    int fd;
    MemoryPool* memory_pool;
    int parent_pid;
private:
    void SetSecurityContext();
};

struct GatewayClient {
    uv_tcp_t* tcp_handle;
    HttpServer* server;
    SSL* ssl_handle;
    BIO* read_bio;
    BIO* write_bio;
    HttpParser *http_parser;
    MemoryPoolTicket *ticket;
    bool finished;
    std::map<const char*, Field*> fields;
    unsigned int resolved_fields = 0;
    unsigned int fields_to_resolve = 0;
    std::map<const char*, std::vector<TextSpan*>> fields_result;
    std::vector<FragmentDefinition*>* fragments;
};

struct BackendEndpoint {
    const char* origin;
    const char* host;
    const char* hostname;
    unsigned int port;
    const char* path;
};

struct CompareStrings {
    bool operator()(const char *a, const char *b) const {
        return std::strcmp(a, b) < 0;
    }
};

extern std::map<const char*, BackendEndpoint, CompareStrings> field_to_endpoint;

struct GraphQlFieldRequest {
    const char* field;
    const char* hostname;
    unsigned int port;
    const char* host;
    const char* path;
    uv_tcp_t *tcp_handle;
    GatewayClient* gateway_client;
};

void OnGatewayClientRequestRead(uv_stream_t *client_stream, ssize_t read_length, const uv_buf_t *buf);

void OnNewConnection(uv_stream_t *server, int status);

void AllocateBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

}


#endif //FLASHPOINT_HTTP_SERVER_H

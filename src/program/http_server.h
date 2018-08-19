#ifndef FLASHPOINT_HTTP_SERVER_H
#define FLASHPOINT_HTTP_SERVER_H

#include <uv.h>
#include <openssl/ssl.h>
#include <program/http_parser.h>
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
    void Close();

    uv_loop_t* loop;
    SSL_CTX* ssl_ctx;
    MemoryPool* memory_pool;
    int parent_pid;
private:
    void SetSecurityContext();
};

struct GatewayClient {
    uv_tcp_t* tcp_handle;
    HttpServer* server;
    std::map<const char*, Field*> fields;
    std::vector<FragmentDefinition*>* fragments;
    SSL* ssl_handle;
    BIO* read_bio;
    BIO* write_bio;
};

struct BackendEndpoint {
    const char* origin;
    const char* host;
    const char* hostname;
    unsigned int port;
    const char* path;
};

struct cmp_str {
    bool operator()(const char *a, const char *b) const {
        return std::strcmp(a, b) < 0;
    }
};

extern std::map<const char*, BackendEndpoint, cmp_str> field_to_endpoint;

struct ClientRequest {
    const char* hostname;
    unsigned int port;
    const char* host;
    const char* path;
    uv_tcp_t *tcp_handle;
    GatewayClient* gateway_client;
};

void on_read(uv_stream_t *client_stream, ssize_t length, const uv_buf_t *buf);

void echo_write(uv_write_t* req, int status);

void OnNewConnection(uv_stream_t *server, int status);

void AllocateBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

}


#endif //FLASHPOINT_HTTP_SERVER_H

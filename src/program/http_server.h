#ifndef FLASHPOINT_HTTP_SERVER_H
#define FLASHPOINT_HTTP_SERVER_H

#include <uv.h>
#include <openssl/ssl.h>
#include <program/http_parser.h>
#include <lib/memory_pool.h>
#include <vector>

namespace flashpoint::program {
class HttpServer {
public:

    HttpServer(uv_loop_t* loop);

    void
    listen(const char *host, unsigned int port);

    void
    close();

    uv_loop_t*
    loop;

    SSL_CTX*
    ssl_ctx;

    MemoryPool*
    memory_pool;

    int
    parent_pid;

private:

    void
    setup_security_context();

};

struct Client {
    uv_tcp_t* socket;
    HttpServer* server;
    std::vector<char> buffer_out;
    SSL* ssl;
    BIO* read_bio;
    BIO* write_bio;
};

void
on_read(uv_stream_t *client_stream, ssize_t length, const uv_buf_t *buf);

void
echo_write(uv_write_t* req, int status);

void
on_new_connection(uv_stream_t* server, int status);

void
alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

}


#endif //FLASHPOINT_HTTP_SERVER_H

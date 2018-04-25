#include <program/http_server.h>
#include <program/http_parser.h>
#include <uv.h>

namespace flashpoint::program {

    static sockaddr_in addr;


    void on_new_connection(uv_stream_t* server, int status)
    {
        if (status < 0) {
            std::fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        }
        uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_tcp_init(((HttpServer*)server->data)->loop, client);
        if (uv_accept(server, (uv_stream_t*)client) == 0) {
            uv_read_start((uv_stream_t*)client, alloc_buffer, echo_read);
        }
        else {
            uv_close((uv_handle_t*)client, NULL);
        }
    }

    void handle_signal(uv_signal_t* signal, int signum)
    {
        exit(0);
    }

    void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf);
    void echo_write(uv_write_t* req, int status);

    HttpServer::HttpServer(uv_loop_t* loop):
        loop(loop) { }

    int HttpServer::listen(const char *host, unsigned int port)
    {
        uv_signal_t* signal = (uv_signal_t*)malloc(sizeof(uv_signal_t));
        uv_signal_init(loop, signal);
        uv_signal_start(signal, handle_signal, SIGHUP);
        uv_tcp_t* server = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_tcp_init(loop, server);
        server->data = this;
        uv_ip4_addr(host, port, &addr);
        uv_tcp_bind(server, (sockaddr*)&addr, 0);
        int r = uv_listen((uv_stream_t*)server, 128, on_new_connection);
        if (r) {
            std::fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        }
    }

    void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
    {
        if (nread < 0) {
            if (nread != UV_EOF) {
                std::fprintf(stderr, "Read error %s\n", uv_err_name(nread));
                uv_close((uv_handle_t*)client, NULL);
            }
        }
        else if (nread > 0) {
            uv_write_t *req = (uv_write_t*)malloc(sizeof(uv_write_t));
            uv_buf_t wrbuf = uv_buf_init(buf->base, nread);
            HttpParser http_parser(buf->base, nread);
            std::unique_ptr<HttpRequest> request = http_parser.parse();
            if (request == nullptr) {
                uv_close((uv_handle_t*)client, NULL);
                return;
            }
            uv_write(req, client, &wrbuf, 1, echo_write);
            uv_close((uv_handle_t*)client, NULL);
        }

        if (buf->base) {
            delete buf->base;
        }
    }

    void echo_write(uv_write_t* req, int status)
    {
        if (status) {
            fprintf(stderr, "Write error %s\n", uv_strerror(status));
        }
        free(req);
    }

    void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
        buf->base = new char[suggested_size];
        buf->len = suggested_size;
    }
}
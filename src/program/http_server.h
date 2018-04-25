#ifndef FLASHPOINT_HTTP_SERVER_H
#define FLASHPOINT_HTTP_SERVER_H

#include <uv.h>

namespace flashpoint::program {
    class HttpServer {
    public:
        HttpServer(uv_loop_t* loop);
        int listen(const char *host, unsigned int port);
        void close();
        uv_loop_t* loop;
    private:
    };

    void echo_read(uv_stream_t* client, ssize_t nread, const uv_buf_t *buf);
    void echo_write(uv_write_t* req, int status);
    void on_new_connection(uv_stream_t* server, int status);
    void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
}


#endif //FLASHPOINT_HTTP_SERVER_H

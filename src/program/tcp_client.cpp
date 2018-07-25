#include "tcp_client.h"
#include <functional>
#include <future>
#include <uv.h>

namespace flashpoint::program {
    void on_close(uv_handle_t* handle)
    {
        printf("closed.");
    }

    TcpClient::TcpClient(uv_loop_t* loop):
        loop(loop),
        on_data_cb([](const char* data) {})
    { }

    void TcpClient::connect(const char *host, unsigned int port)
    {
        if (port < MIN_PORT || port > MAX_PORT) {
            throw std::domain_error("Port must be between 1 and 65535.");
        }
        uv_tcp_t* socket = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_tcp_init(loop, socket);
        uv_tcp_keepalive(socket, 1, 60);
        sockaddr_in* dest = (sockaddr_in*)malloc(sizeof(sockaddr_in));
        uv_ip4_addr(host, port, dest);
        uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
        connect->data = this;
        uv_tcp_connect(connect, socket, (sockaddr*)dest, [](uv_connect_s* _connection, int status) {
            ((TcpClient*)_connection->data)->connection = _connection;
        });
        uv_run(loop, UV_RUN_DEFAULT);
    }

    void TcpClient::on_data(std::function <void(const char *)> on_data)
    {
        uv_read_start(connection->handle, [](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
            buf->base = new char[suggested_size];
            buf->len = suggested_size;
        }, [](uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf) {
            if (nread >= 0) {
                ((TcpClient*)tcp->data)->on_data_cb(buf->base);
            }
        });
        on_data_cb = on_data;
    }

    void TcpClient::send(const char* message)
    {
        uv_buf_t buffer[] = {
            { (char*)message, strlen(message) },
        };
        uv_write_t request;
        uv_write(&request, connection->handle, buffer, 2, [](uv_write_t* req, int status) { });
    }

    std::future<const char*> TcpClient::read()
    {
        uv_read_start(connection->handle, [](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
            buf->base = new char[suggested_size];
            buf->len = suggested_size;
        }, [](uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf) {
            if (nread >= 0) {
                ((TcpClient*)tcp->data)->read_promise.set_value(buf->base);
            }
            uv_close((uv_handle_t*)tcp, on_close);
            delete buf->base;
        });
        return read_promise.get_future();
    }
}
#ifndef FLASHPOINT_TCP_CLIENT_H
#define FLASHPOINT_TCP_CLIENT_H

#include <stdexcept>
#include <functional>
#include <future>
#include <uv.h>

#define MAX_PORT 65535
#define MIN_PORT 1

namespace flashpoint::program {

    struct Connection {
        uv_stream_t* _connection;
    };

    class TcpClient {
    public:
        TcpClient(uv_loop_t* loop);
        void connect(const char* host, unsigned int port);
        void send(const char* message);
        void on_data(std::function<void(const char*)>);
        std::future<const char*> read();
    private:
        std::promise<const char*> read_promise;
        uv_connect_s* connection;
        uv_loop_t* loop;
        std::function<void(const char*)> on_data_cb;
    };

}


#endif //FLASHPOINT_TCP_CLIENT_H

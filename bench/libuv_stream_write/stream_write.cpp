#include <uv.h>
#include <memory>
#include <iostream>
#include <chrono>
#include <cmath>

struct Data {
    std::size_t finished;

    Data(std::size_t finished):
        finished(finished) { }
};

struct Req {
    Data* data;
    uv_buf_t buf;

    Req(Data* data): data(data) { }
};

static uv_loop_t *default_loop;
std::size_t data_size = 0;
std::size_t buffer_size = 0;
std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();


void on_write_cb(uv_write_t* write_request, int status) {
    if (status < 0) {
        std::cerr << uv_err_name(status) << std::endl;
    }
    ((Req*)write_request->data)->data->finished += ((Req*)write_request->data)->buf.len;
    Data* data = ((Req*)write_request->data)->data;
    if (data->finished >= data_size) {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "Duration: " << duration << "us" << std::endl;
        uv_close((uv_handle_t*)write_request->handle, NULL);
    }
    delete write_request;
}

void do_write(uv_stream_t *client_stream, Data* data, char* str, std::size_t size) {
    auto write_request = (uv_write_t*)malloc(sizeof(uv_write_t));
    auto req = new Req(data);
    req->buf = uv_buf_init(str, size);
    write_request->data = req;
    int r = uv_write(write_request, client_stream, &req->buf, 1, OnWriteEnd);
    if (r < 0) {
        printf("ERROR: WriteToSocket erro");
    }
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}


void on_read(uv_stream_t *client_stream, ssize_t length, const uv_buf_t *buf) {
    start = std::chrono::steady_clock::now();
    auto data = new Data(0);

    std::size_t remainder = data_size % buffer_size;
    std::size_t count = data_size / buffer_size;
    if (remainder > 0) {
        count++;
    }
    for (std::size_t y = 0; y < count; y++) {
        char *big_str = (char*)malloc(buffer_size);
        for (std::size_t i = 0; i < buffer_size ; i++) {
            big_str[i] = 'r';
        }
        do_write(client_stream, data, big_str, buffer_size);
    }

    if (buf->base) {
        delete buf->base;
    }
}


void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    }
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(default_loop, client);
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        int r = uv_read_start((uv_stream_t *) client, AllocateBuffer, OnRead);
        if(r == -1) {
            printf("ERROR: uv_read_start error: %s\n", uv_strerror(r));
            ::exit(0);
        }
    }
    else {
        uv_close((uv_handle_t*)client, NULL);
    }
}

int main(int argc, char** argv) {
    data_size = std::atoi(argv[1]);
    buffer_size = std::atoi(argv[2]);
    default_loop = uv_default_loop();
    uv_tcp_t* server = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(default_loop, server);
    sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", 8000, &addr);
    uv_tcp_bind(server, (sockaddr*)&addr, 0);

    int r = uv_listen((uv_stream_t *) server, 128, OnNewConnection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    }
    uv_run(default_loop, UV_RUN_DEFAULT);
}

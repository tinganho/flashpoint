#include <iostream>
#include <program/http_server.h>
#include <uv.h>

using namespace flashpoint::program;

int main(int argc, char* argv[]) {
    uv_loop_t* loop = uv_default_loop();
    HttpServer server(loop);
    server.listen("0.0.0.0", 8000);
    return uv_run(loop, UV_RUN_DEFAULT);
}
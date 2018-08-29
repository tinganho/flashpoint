#include <iostream>
#include <program/HttpServer.h>
#include <uv.h>

using namespace flashpoint;
using namespace flashpoint::program;

int main(int argc, char* argv[]) {
    uv_loop_t* loop = uv_default_loop();
    HttpServer server(loop);
    server.Listen("0.0.0.0", 8000);
    return uv_run(loop, UV_RUN_DEFAULT);
}
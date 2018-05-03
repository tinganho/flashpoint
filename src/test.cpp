#include <iostream>
#include <test/baseline_test_runner.h>
#include <future>
#include <uv.h>

using namespace flashpoint::test;

int main(int argc, char* argv[]) {
    uv_loop_t* loop = uv_default_loop();
    BaselineTestRunner test_runner(loop);
    test_runner.run_graphql_tests();
//    if (argc > 1) {
//        if(strcmp(argv[1], "--no-server") == 0) {
//            test_runner.run_http_tests();
//        }
//    }
//    else {
//        test_runner.run_http_tests_with_server();
//    }
    return 0;
}

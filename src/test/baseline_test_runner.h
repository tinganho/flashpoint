#ifndef FLASH_BASELINE_TEST_RUNNER_H
#define FLASH_BASELINE_TEST_RUNNER_H

#include <string>
#include <functional>
#include <boost/filesystem/path.hpp>
#include <uv.h>

namespace flashpoint::test {
    class BaselineTestRunner {
    public:
        BaselineTestRunner(uv_loop_t* loop);
        void run();
        int start_server();
        int run_tests();
    private:
        uv_loop_t* loop;
        void visit_tests(std::function<void(const char*, boost::filesystem::path, std::string)> callback);
    };
}

#endif //FLASH_BASELINE_TEST_RUNNER_H

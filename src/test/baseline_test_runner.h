#ifndef FLASH_BASELINE_TEST_RUNNER_H
#define FLASH_BASELINE_TEST_RUNNER_H

#include <string>
#include <functional>
#include <boost/filesystem/path.hpp>
#include <uv.h>

using namespace boost::filesystem;

namespace flashpoint::test {
    struct TestCase {
        std::string test_case_file_name;
        const char* test_case_file_content;
        const char* reference_file_content;
        const path& current_file_path;
        const path& reference_file_path;
    };

    class BaselineTestRunner {
    public:
        BaselineTestRunner(uv_loop_t* loop);
        void run_http_tests_with_server();
        void start_server();
        void run_http_tests();
        void run_graphql_tests();
    private:
        uv_loop_t* loop;
        void visit_tests(const path& test_path, std::function<void(const TestCase& test_case)> callback);
        bool assert_baselines(const char* current, const char* reference, std::stringstream& error_message);
    };
}

#endif //FLASH_BASELINE_TEST_RUNNER_H

#ifndef FLASH_BASELINE_TEST_RUNNER_H
#define FLASH_BASELINE_TEST_RUNNER_H

#include <string>
#include <functional>
#include <lib/text_writer.h>
#include <boost/filesystem/path.hpp>
#include <uv.h>
#include <experimental/optional>

using namespace boost::filesystem;
using namespace flashpoint::lib;

namespace flashpoint::test {
    struct TestCase {
        std::string name;
        const char* source;
        const char* folder;
        const path current_folder;
        const path reference_folder;
    };

    struct RunOption {
        std::experimental::optional<std::string> test;
        std::experimental::optional<std::string> folder;
    };

    class BaselineTestRunner {
    public:
        BaselineTestRunner(uv_loop_t* loop);

        void
        run_http_tests_with_server(const RunOption& option);

        void
        start_server();

        void
        run_http_tests(const RunOption& run_option);

        void
        run_graphql_tests(const RunOption& run_option);

        void
        accept_graphql_tests(const RunOption& run_option);

    private:
        uv_loop_t* loop;
        std::size_t current_line = 1;

        void
        visit_tests_by_path(
            const path& folder,
            std::function<void(const TestCase& test_case)> callback,
            bool delete_folder);

        void
        visit_tests(
            const path& test_folder,
            std::vector<std::string>& tests,
            std::function<void(const TestCase& test_case)> callback);

        bool
        assert_baselines(const char* current, const char* reference, std::stringstream& error_message);

        void
        append_mutation_chunk(const std::string &message, bool is_insertion,
                              TextWriter &tw);

        std::string
        append_all_lines_except_last_line(const std::string &chunk, TextWriter &tw);

        std::tuple<std::string, bool>
        append_all_lines(const std::string &message, TextWriter &tw);

        std::tuple<std::string, std::vector<std::string>, std::size_t>
        get_first_and_rest_lines(const std::string& chunk);

        std::string
        line_number();


        std::string
        line_number(std::size_t line);
    };
}

#endif //FLASH_BASELINE_TEST_RUNNER_H

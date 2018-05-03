#include <iostream>
#include <program/http_server.h>
#include <lib/graphql/graphql_parser.h>
#include <lib/tcp_client_raw.h>
#include <test/baseline_test_runner.h>
#include <test/test_definition.h>
#include <lib/utils.h>
#include <boost/filesystem/path.hpp>
#include <diff_match_patch.h>
#include <algorithm>
#include <string>
#include <sstream>

using namespace flashpoint::lib;
using namespace flashpoint::lib::graphql;
using namespace flashpoint::program;
using namespace boost::filesystem;

namespace flashpoint::test {
    boost::filesystem::path root_folder(root_path());

    BaselineTestRunner::BaselineTestRunner(uv_loop_t* loop):
        loop(loop)
    {
    }


    void handle_sighup(int signum)
    { }

    void BaselineTestRunner::visit_tests(const path& test_folder, std::function<void(const TestCase& test_case)> callback)
    {
        std::vector<std::string> tests = find_files(root_path() / test_folder / "/tests/cases/*");
        for (const auto& t : tests) {
            const char* test_case_file_content = read_file(t);
            std::vector<std::string> paths = split_string(replace_string(t, (test_folder / "cases").string(), ""), '/');
            std::string test_case_file_name = paths.back();

            path current_folder(test_folder / "currents");
            path reference_folder(test_folder / "references");
            path current_file = current_folder / test_case_file_name;
            path reference_file = reference_folder / test_case_file_name;

            if (!path_exists(current_folder)) {
                create_folder(current_folder);
            }
            const char* reference_file_content = "";
            if (path_exists(reference_file)) {
                reference_file_content = read_file(reference_file);
            }
            callback(TestCase {
                test_case_file_name,
                test_case_file_content,
                reference_file_content,
                current_file,
                reference_file,
            });
        }
    }

    void BaselineTestRunner::run_http_tests_with_server() {
        pid_t parent_pid = getpid();
        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "An error occurred when forking process" << std::endl;
            return;
        }

        // Parent process
        if (pid != 0) {
            start_server();
        }

        // Child process
        else {
            run_http_tests();
            kill(parent_pid, SIGHUP);
        }
    }

    void BaselineTestRunner::run_graphql_tests()
    {
        domain("GraphQL");
        visit_tests("src/lib/graphql", [&](const TestCase& test_case) {
            test(test_case.test_case_file_name, [=](Test* test, std::function<void()> success, std::function<void(std::string error)> error) {
                GraphQlParser parser;
                parser.parse(test_case.test_case_file_content);
            });
        });
        run_test_suites();
        print_result();
    }

    void BaselineTestRunner::run_http_tests()
    {
        domain("HTTP");
        visit_tests("src/program", [&](const TestCase& test_case) {
            test(test_case.test_case_file_name, [=](Test* test, std::function<void()> success, std::function<void(std::string error)> error) {
                TcpClientRaw tcp_client;
                tcp_client._connect("0.0.0.0", 8000);
                tcp_client.send_message(test_case.test_case_file_content);
                const char* current_file_content = tcp_client.recieve_message();
                write_file(test_case.current_file_path, current_file_content);
                std::stringstream error_message;
                if (!assert_baselines(current_file_content, test_case.reference_file_content, error_message)) {
                    return error(error_message.str());
                }
                success();
            });
        });
        run_test_suites();
        print_result();
    }

    bool BaselineTestRunner::assert_baselines(const char* current, const char* reference, std::stringstream& error_message)
    {
        if (strcmp(current, reference) != 0) {
            error_message << "\n\e[0m";
            diff_match_patch<std::string> diff;
            std::string c(current);
            std::string r(reference);
            auto diffs = diff.diff_main(r, c);
            diff_match_patch<std::string>::diff_cleanupSemantic(diffs);
            for (const auto& d : diffs) {
                if (d.operation == diff_match_patch<std::string>::Operation::DELETE) {
                    error_message << "\e[31m" << d.text << "\e[0m";
                }
                else if (d.operation == diff_match_patch<std::string>::Operation::INSERT) {
                    error_message << "\e[32m" << d.text << "\e[0m";
                }
                else {
                    error_message << d.text;
                }
            }
            return false;
        }
        return true;
    }

    void BaselineTestRunner::start_server() {
        HttpServer server(loop);
        server.listen("0.0.0.0", 8000);
    }
}

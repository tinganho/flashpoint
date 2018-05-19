#include <iostream>
#include <program/http_server.h>
#include <program/graphql/graphql_executor.h>
#include <lib/tcp_client_raw.h>
#include <test/baseline_test_runner.h>
#include <test/test_definition.h>
#include <lib/utils.h>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <diff_match_patch.h>
#include <algorithm>
#include <string>
#include <sstream>
#include "diagnostic_writer.h"

using namespace flashpoint::lib;
using namespace flashpoint::program;
using namespace flashpoint::program::graphql;
using namespace boost::filesystem;

namespace flashpoint::test {
    boost::filesystem::path root_folder(root_dir());

    BaselineTestRunner::BaselineTestRunner(uv_loop_t* loop):
        loop(loop)
    {
    }

    void BaselineTestRunner::visit_tests_by_path(const path& folder, std::function<void(const TestCase& test_case)> callback, bool delete_folder = true)
    {
        path tests_folder = root_dir() / folder / "/tests";
        path current_folder = tests_folder / "currents";
        if (delete_folder) {
            remove_folder(tests_folder / "currents");
            if (!path_exists(current_folder)) {
                create_folder(current_folder);
            }
        }
        std::vector<std::string> tests = find_files(tests_folder / "cases/*");
        visit_tests(folder, tests, callback);

    }
    void BaselineTestRunner::visit_tests(
        const path& test_folder,
        std::vector<std::string>& tests,
        std::function<void(const TestCase& test_case)> callback)
    {
        for (const auto& test : tests) {
            path t = boost::filesystem::weakly_canonical(test);
            if (is_directory(t)) {
                auto files = find_files(t / "*");
                visit_tests(test_folder, files, callback);
            }
            else {
                const char* content = read_file(t);
                std::string relative_test_path = replace_string(t.string(), root_dir().string() + "/", "");
                std::vector<std::string> paths = split_string(relative_test_path, '/');
                std::string filename = paths.back();
                std::string name = split_string(filename, '.').front();

                paths.pop_back();

                std::string s = join_vector(paths, "/");
                std::string f = (test_folder / "tests/cases").string();
                std::string sub_folder = replace_string(s, f, "");
                if (sub_folder.size() > 0) {
                    sub_folder = sub_folder.substr(1); // Remove '/'
                }
                path current_folder = root_dir() / test_folder / "tests/currents" / sub_folder;
                path reference_folder = root_dir() / test_folder / "tests/references" / sub_folder;

                if (!path_exists(current_folder)) {
                    create_folder(current_folder);
                }

                callback(TestCase {
                    name,
                    content,
                    sub_folder.c_str(),
                    current_folder,
                    reference_folder
                });
            }
        }
    }

    void BaselineTestRunner::run_http_tests_with_server(const RunOption& run_option) {
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
            run_http_tests(run_option);
            kill(parent_pid, SIGHUP);
        }
    }

    void BaselineTestRunner::accept_graphql_tests(const RunOption& run_option)
    {
        visit_tests_by_path("src/program/graphql", [&](const TestCase test_case) {
            if (run_option.folder && *run_option.folder != test_case.folder) {
                return;
            }
            if (run_option.test && *run_option.test != test_case.name) {
                return;
            }
            auto current_error_file = test_case.current_folder / (test_case.name + ".errors");
            if (exists(current_error_file)) {
                if (!exists(test_case.reference_folder)) {
                    create_folder(test_case.reference_folder);
                }
                auto reference_error_file = test_case.reference_folder / (test_case.name + ".errors");
                auto current_error_file_source = read_file(current_error_file);
                write_file(reference_error_file, current_error_file_source);
            }
        }, false);
    }

    void BaselineTestRunner::run_graphql_tests(const RunOption& run_option)
    {
        domain("GraphQL");
        visit_tests_by_path("src/program/graphql", [&](const TestCase test_case) {
            if (run_option.folder && *run_option.folder != test_case.folder) {
                return;
            }
            if (run_option.test && *run_option.test != test_case.name) {
                return;
            }
            test(test_case.name, [=](Test* test, std::function<void()> done, std::function<void(std::string error)> error) {
                auto memory_pool = new MemoryPool(1024 * 4 * 10000, 1024 * 4);
                auto ticket = memory_pool->take_ticket();
                GraphQlParser parser(memory_pool, ticket);
                std::vector<Glib::ustring> source;
                boost::algorithm::split_regex(source, test_case.source, boost::regex("====\n"));
                DiagnosticWriter* diagnostic_writer;
                if (source.size() > 1) {
                    diagnostic_writer = new DiagnosticWriter(
                        parser.add_schema(&source[0]), parser.execute(&source[1]));
                }
                else {
                    diagnostic_writer = new DiagnosticWriter(
                        parser.add_schema(&source[0]));
                }
                auto current_errors_file_source = diagnostic_writer->to_string();
                write_file(test_case.current_folder / (test_case.name + ".errors"), current_errors_file_source.c_str());
                std::stringstream error_message;
                auto reference_error_file = test_case.reference_folder / (test_case.name + ".errors");
                const char* reference_errors_file_source = "";
                if (exists(reference_error_file)) {
                    reference_errors_file_source = read_file(reference_error_file);
                }
                if (!assert_baselines(current_errors_file_source.c_str(), reference_errors_file_source, error_message)) {
                    error(error_message.str());
                }
                memory_pool->return_ticket(ticket);
                done();
            });
        });
        run_test_suites();
        print_result();
    }

    void BaselineTestRunner::run_http_tests(const RunOption& run_option)
    {
        domain("HTTP");
        visit_tests_by_path("src/program", [&](const TestCase& test_case) {
            if (run_option.folder && *run_option.folder != test_case.folder) {
                return;
            }
            if (run_option.test && *run_option.test != test_case.name) {
                return;
            }
            test(test_case.name, [=](Test* test, std::function<void()> success, std::function<void(std::string error)> error) {
                TcpClientRaw tcp_client;
                tcp_client._connect("0.0.0.0", 8000);
                tcp_client.send_message(test_case.source);
                const char* current_file_content = tcp_client.recieve_message();
                write_file(test_case.current_folder, current_file_content);
                std::stringstream error_message;
//                if (!assert_baselines(current_file_content, test_case.reference_folder, error_message)) {
//                    return error(error_message.str());
//                }
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

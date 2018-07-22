#include <iostream>
#include <program/http_server.h>
#include <program/graphql/graphql_executor.h>
#include <program/graphql/graphql_schema.h>
#include <lib/tcp_client_raw.h>
#include <lib/text_writer.h>
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
#include "test_case_scanner.h"
#include <regex>

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
            path test_path = boost::filesystem::weakly_canonical(test);
            if (is_directory(test_path)) {
                auto files = find_files(test_path / "*");
                visit_tests(test_folder, files, callback);
            }
            else {
                const char* content = read_file(test_path);
                TestCaseScanner scanner(content);
                auto test_cases = scanner.scan();
//                for (const auto& d : scanner.diagnostics) {
//
//                }
                if (test_cases.empty()) {
                    call_test(test_folder, test_path, test_path, scanner.get_source_code({}), callback);
                }
                else {
                    for (const auto& test_case : test_cases) {
                        Glib::ustring source_code = scanner.get_source_code(test_case->source_code_arguments);
                        const std::string test_path_string = test_path.string();
                        boost::regex rgx("\\{\\w+\\}");
                        boost::smatch match;
                        if (!boost::regex_search(test_path_string.begin(), test_path_string.end(), match, rgx)) {
                            throw std::logic_error("The path '" + test_path_string + "' must contain template arguments.");
                        }
                        path new_test_path = scanner.get_filename(test_path_string, test_case->file_arguments);
                        call_test(test_folder, new_test_path, test_path, source_code, callback);
                    }
                }
            }
        }
    }

    void
    BaselineTestRunner::call_test(
        path test_folder,
        path test_path,
        path aggregate_path,
        Glib::ustring source_code,
        const std::function<void(const TestCase& test_case)>& callback)
    {
        std::string relative_test_path = replace_string(test_path.string(), root_dir().string() + "/", "");
        std::vector<std::string> test_paths = split_string(relative_test_path, '/');
        std::string filename = test_paths.back();
        std::string name = split_string(filename, '.').front();

        std::string relative_aggregate_path = replace_string(aggregate_path.string(), root_dir().string() + "/", "");
        std::vector<std::string> aggregate_paths = split_string(relative_aggregate_path, '/');
        std::string aggregate_name = split_string(aggregate_paths.back(), '.').front();

        test_paths.pop_back();

        std::string s = join_vector(test_paths, "/");
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
            aggregate_name,
            source_code,
            sub_folder,
            current_folder,
            reference_folder
        });
    }


    void
    BaselineTestRunner::run_http_tests_with_server(const RunOption& run_option)
    {
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

    void
    BaselineTestRunner::accept_graphql_tests(const RunOption& run_option)
    {
        visit_tests_by_path("src/program/graphql", [&](const TestCase& test_case) {
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
                if (exists(reference_error_file)) {
                    remove(reference_error_file);
                }
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
            auto s = *run_option.test;
            if (run_option.test && (s != test_case.aggregate_name && s != test_case.name)) {
                return;
            }
            test(test_case.name, [=](Test* test, std::function<void()> done, std::function<void(std::string error)> error) {
                auto memory_pool = new MemoryPool(1024 * 4 * 10000, 1024 * 4);
                auto ticket = memory_pool->take_ticket();
                std::vector<Glib::ustring> source;
                boost::algorithm::split_regex(source, test_case.source, boost::regex("====\n"));
                DiagnosticWriter diagnostic_writer;
                GraphQlSchema schema(source[0], memory_pool, ticket);
                GraphQlExecutor executor(memory_pool, ticket);
                executor.add_schema(schema);
                diagnostic_writer.add_diagnostics(schema.diagnostics, schema.source);
                if (source.size() > 1) {
                    diagnostic_writer.add_source("====\n");
                    diagnostic_writer.add_diagnostics(executor.execute(source[1])->diagnostics, source[1]);
                }
                auto current_errors_file_source = diagnostic_writer.to_string();
                auto current_error_file = test_case.current_folder / (test_case.name + ".errors");
                write_file(current_error_file, current_errors_file_source.c_str());
                std::stringstream error_message;
                auto reference_error_file = test_case.reference_folder / (test_case.name + ".errors");
                const char* reference_errors_file_source = "";
                bool reference_file_exists = false;
                if (exists(reference_error_file)) {
                    reference_errors_file_source = read_file(reference_error_file);
                    reference_file_exists = true;
                }
                if (!assert_baselines(current_errors_file_source.c_str(), reference_errors_file_source, error_message)) {
                    if (reference_file_exists) {
                        auto command = std::string("git diff --no-index --color \"") + reference_error_file.c_str() + "\" \"" + current_error_file.c_str() + "\" | cat";
                        error(execute_command(command));
                    }
                    else {
                        error("\n" + current_errors_file_source);
                    }
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
                tcp_client.send_message(test_case.source.c_str());
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

    std::tuple<std::string, std::vector<std::string>, std::size_t>
    BaselineTestRunner::get_first_and_rest_lines(const std::string& chunk)
    {
        std::stringstream line;
        std::string first_line;
        std::vector<std::string> rest_lines;
        bool has_encountered_first_line = false;
        std::size_t n = 1;
        for (std::size_t i = 0; i < chunk.size(); i++) {
            char ch = chunk[i];
            if (ch == '\n') {
                if (!has_encountered_first_line) {
                    first_line = line.str();
                    has_encountered_first_line = true;
                }
                else {
                    rest_lines.push_back(line.str());
                }
                line.str("");
                n++;
            }
            else if (ch == '\r') {
                if (chunk[i + 1] == '\n') {
                    i++;
                }
                if (!has_encountered_first_line) {
                    first_line = line.str();
                    has_encountered_first_line = true;
                }
                else {
                    rest_lines.push_back(line.str());
                }
                line.str("");
                n++;
            }
            else {
                line << ch;
            }
        }
        if (!has_encountered_first_line) {
            first_line = line.str();
        }
        else {
            rest_lines.push_back(line.str());
        }
        return std::make_tuple(first_line, rest_lines, n);
    };

    std::string BaselineTestRunner::line_number()
    {
        return line_number(current_line);
    }


    std::string BaselineTestRunner::line_number(std::size_t line)
    {
        if (line < 10) {
            return "\e[90m   " + std::to_string(line) + "  \e[0m ";
        }
        else if (line < 100) {
            return "\e[90m  " + std::to_string(line) + " \e[0m  ";
        }
        else if (line < 1000) {
            return "\e[90m " + std::to_string(line) + " \e[0m   ";
        }
        else {
            throw std::logic_error("Your file is too big");
        }
    }

    void BaselineTestRunner::append_mutation_chunk(
        const std::string &message,
        bool is_insertion,
        TextWriter &tw)
    {
        std::stringstream line;
        std::size_t n = 0;
        for (std::size_t i = 0; i < message.size(); i++) {
            char ch = message[i];
            if (ch == '\n') {
                if (n > 0) {
                }
                if (is_insertion) {
                    tw.write("\e[32m" + line.str() + "\e[0m");
                }
                else {
                    tw.write("\e[31m" + line.str() + "\e[0m");
                }
                tw.newline();
                if (is_insertion) {
                    tw.write(line_number(++current_line));
                }
                else {
                    tw.write(line_number(current_line + n));
                }
                line.str("");
                n++;
            }
            else if (ch == '\r') {
                if (message[i + 1] == '\n') {
                    i++;
                }
                if (is_insertion) {
                    tw.write("\e[32m" + line.str() + "\e[0m");
                }
                else {
                    tw.write("\e[31m" + line.str() + "\e[0m");
                }
                tw.newline();
                if (is_insertion) {
                    tw.write(line_number(++current_line));
                }
                else {
                    tw.write(line_number(current_line + n));
                }
                line.str("");
                n++;
            }
            else {
                line << ch;
            }
        }
        if (is_insertion) {
            tw.write("\e[32m" + line.str() + "\e[0m");
        }
        else {
            tw.write("\e[31m" + line.str() + "\e[0m");
        }
    }

    std::tuple<std::string, bool> BaselineTestRunner::append_all_lines(const std::string &chunk, TextWriter &tw)
    {
        std::stringstream line;
        std::size_t n = 0;
        for (std::size_t i = 0; i < chunk.size(); i++) {
            char ch = chunk[i];
            if (ch == '\n') {
                n++;
                tw.write(line.str());
                line.str("");
            }
            else if (ch == '\r') {
                if (chunk[i + 1] == '\n') {
                    i++;
                }
                tw.write(line.str());
                line.str("");
                n++;
            }
            else {
                line << ch;
            }
        }
        tw.write(line.str());
        return std::make_tuple(*tw.text, n > 0);
    }

    bool BaselineTestRunner::assert_baselines(const char* current, const char* reference, std::stringstream& error_message)
    {
        if (strcmp(current, reference) != 0) {
            return false;
        }
        return true;
    }


    void BaselineTestRunner::start_server() {
        HttpServer server(loop);
        server.listen("0.0.0.0", 8000);
    }
}

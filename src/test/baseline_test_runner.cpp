#include <iostream>
#include <program/http_server.h>
#include <program/graphql/graphql_executor.h>
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
            if (run_option.test && s != test_case.name) {
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
//            auto tw = TextWriter();
//            tw.write("\n\e[0m");
//            diff_match_patch<std::string> diff;
//            std::string c(current);
//            std::string r(reference);
//            auto diffs = diff.diff_main(r, c);
//            std::string last_line;
//            diff_match_patch<std::string>::diff_cleanupSemantic(diffs);
//            enum class ChunkType {
//                Equal,
//                Delete,
//                Insert,
//            } last_chunk_type = ChunkType::Equal;
//            bool has_appended_last_line = false;
//            bool last_sequence_was_delete = false;
//            bool last_sequence_was_delete_insert = false;
//            tw.write(line_number(current_line++));
//            std::string first_line_after_delete;
//
//            for (const auto& d : diffs) {
//                if (d.operation == diff_match_patch<std::string>::Operation::DELETE) {
//                    tw.write(last_line);
//                    append_mutation_chunk(d.text, false, tw);
//                    tw.add_placeholder("delete");
//                    last_sequence_was_delete = true;
//                    has_appended_last_line = true;
//                }
//                else if (d.operation == diff_match_patch<std::string>::Operation::INSERT) {
//                    if (last_sequence_was_delete) {
//                        current_line--;
//                        tw.newline();
//                        tw.write(line_number());
//                        last_sequence_was_delete = false;
//                        last_sequence_was_delete_insert = true;
//                    }
//                    tw.write(last_line);
//                    append_mutation_chunk(d.text, true, tw);
//                    has_appended_last_line = true;
//                }
//                else {
//                    if (last_sequence_was_delete) {
//                        tw.write(d.text);
//                        tw.add_placeholder("delete");
//                        last_line += d.text;
//                    }
//                    else if (last_sequence_was_delete_insert) {
//                        auto [first_line, rest_lines, number_of_lines] = get_first_and_rest_lines(d.text);
//                        tw.begin_write_on_placeholder("delete");
//                        tw.write(first_line);
//                        tw.end_write_on_placeholder();
//                        if (number_of_lines == 1) {
//                            last_line = first_line;
//                        }
//                        else {
//                            tw.newline();
//                            tw.write(line_number(++current_line));
//                            std::size_t n = 0;
//                            for (const auto& line : rest_lines) {
//                                tw.write(line);
//                                tw.save();
//                                tw.newline();
//                                tw.write(line_number(++current_line));
//                                n++;
//                            }
//                            if (n > 0) {
//                                tw.restore();
//                            }
//                        }
//                        last_sequence_was_delete_insert = false;
//                    }
//                    else {
//                        last_line = append_all_lines_except_last_line(d.text, tw);
//                        has_appended_last_line = false;
//                    }
//                }
//            }
//            if (!has_appended_last_line) {
//                error_message << last_line;
//            }
//            error_message << *tw.text;
//            current_line = 1;
            return false;
        }
        return true;
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

    std::string BaselineTestRunner::append_all_lines_except_last_line(const std::string &chunk, TextWriter &tw)
    {
        std::stringstream line;
        std::vector<std::string> lines;
        std::size_t n = 0;
        bool has_added_lines = false;
        for (std::size_t i = 0; i < chunk.size(); i++) {
            char ch = chunk[i];
            if (ch == '\n') {
                if (has_added_lines) {
                    tw.write_line(line.str());
                    tw.write(line_number(current_line++));
                    line.str("");
                }
                else {
                    tw.newline();
                    tw.write(line_number(current_line++));
                }
                n++;
            }
            else if (ch == '\r') {
                if (chunk[i + 1] == '\n') {
                    i++;
                }
                if (has_added_lines) {
                    tw.write_line(line.str());
                    tw.write(line_number(current_line++));
                    line.str("");
                }
                else {
                    tw.newline();
                    tw.write(line_number(current_line++));
                }
                n++;
            }
            else {
                line << ch;
                has_added_lines = true;
            }
        }
        return line.str();
    }

    void BaselineTestRunner::start_server() {
        HttpServer server(loop);
        server.listen("0.0.0.0", 8000);
    }
}

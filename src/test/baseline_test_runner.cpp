#include <iostream>
#include <program/HttpServer.h>
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
#include <algorithm>
#include <string>
#include <sstream>
#include "diagnostic_writer.h"
#include "GraphQlTestCaseScanner.h"
#include "ServerTestScanner.h"
#include <regex>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <stdio.h>
#include <lib/text_span.h>

using namespace flashpoint::lib;
using namespace flashpoint::program;
using namespace flashpoint::program::graphql;
using namespace boost::filesystem;

namespace flashpoint::test {
    boost::filesystem::path root_folder(root_dir());
    static char errorBuffer[CURL_ERROR_SIZE];
    static std::string buffer;

BaselineTestRunner::BaselineTestRunner(const BaselineTestRunnerOption &option):
    option(option)
{ }

void BaselineTestRunner::VisistTestsByPath(
    const path &folder,
    std::function<void(const TestCase &test_case)> callback,
    bool delete_folder = true)
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
    VisitTests(folder, tests, callback);
}

void BaselineTestRunner::VisitTests(
    const path &test_folder,
    std::vector<std::string> &tests,
    std::function<void(const TestCase &test_case)> callback)
{
    for (const auto& test : tests) {
        path test_path = boost::filesystem::weakly_canonical(test);
        if (is_directory(test_path)) {
            auto files = find_files(test_path / "*");
            VisitTests(test_folder, files, callback);
        }
        else {
            const char* content = read_file(test_path);
            auto ext = extension(test_path);
            GraphQlTestCaseScanner scanner(content);
            auto test_cases = scanner.Scan();
            if (test_cases.empty()) {
                CallTest(test_folder, test_path, test_path, scanner.GetSourceCode({}), callback);
            }
            else {
                for (const auto& test_case : test_cases) {
                    Glib::ustring source_code = scanner.GetSourceCode(test_case->source_code_arguments);
                    const std::string test_path_string = test_path.string();
                    boost::regex rgx("\\{\\w+\\}");
                    boost::smatch match;
                    if (!boost::regex_search(test_path_string.begin(), test_path_string.end(), match, rgx)) {
                        throw std::logic_error("The path '" + test_path_string + "' must contain template arguments.");
                    }
                    path new_test_path = scanner.GetFilename(test_path_string, test_case->file_arguments);
                    CallTest(test_folder, new_test_path, test_path, source_code, callback);
                }
            }
        }
    }
}

void
BaselineTestRunner::CallTest(
    path test_folder,
    path test_path,
    path aggregate_path,
    std::string source_code,
    const std::function<void(const TestCase &test_case)> &callback)
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

void BaselineTestRunner::AcceptGraphQlTests() {
    VisistTestsByPath("src/program/graphql", [&](const TestCase &test_case) {
        if (option.folder && *option.folder != test_case.folder) {
            return;
        }
        if (option.test && *option.test != test_case.name) {
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

void
BaselineTestRunner::DefineGraphQlTests() {
    domain("GraphQL");
    VisistTestsByPath("src/program/graphql", [&](const TestCase test_case) {
        if (option.folder != nullptr && *option.folder != test_case.folder) {
            return;
        }
        if (option.test != nullptr && (*option.test != test_case.aggregate_name && *option.test != test_case.name)) {
            return;
        }
        test(test_case.name, [=](Test *test, std::function<void()> done, std::function<void(std::string error)> error) {
            auto memory_pool = new MemoryPool(1024 * 4 * 10000, 1024 * 4);
            auto ticket = memory_pool->TakeTicket();
            std::vector<Glib::ustring> source;
            boost::algorithm::split_regex(source, test_case.source, boost::regex("====\n"));
            DiagnosticWriter diagnostic_writer;
            GraphQlSchema schema(source[0], memory_pool, ticket);
            GraphQlExecutor executor(memory_pool, ticket);
            executor.AddSchema(schema);
            diagnostic_writer.add_diagnostics(schema.diagnostics, schema.source);
            if (source.size() > 1) {
                diagnostic_writer.add_source("====\n");
                diagnostic_writer.add_diagnostics(executor.Execute(source[1])->diagnostics, source[1]);
            }
            auto current_errors_file_source = diagnostic_writer.to_string();
            auto current_error_file = test_case.current_folder / (test_case.name + ".errors");
            write_file(current_error_file, current_errors_file_source.c_str());
            std::stringstream error_message;
            auto reference_error_file = test_case.reference_folder / (test_case.name + ".errors");
            const char *reference_errors_file_source = "";
            bool reference_file_exists = false;
            if (exists(reference_error_file)) {
                reference_errors_file_source = read_file(reference_error_file);
                reference_file_exists = true;
            }
            if (!assert_baselines(current_errors_file_source.c_str(), reference_errors_file_source, error_message)) {
                if (reference_file_exists) {
                    auto command =
                      std::string("git diff --no-index --color \"") + reference_error_file.c_str() + "\" \"" +
                      current_error_file.c_str() + "\" | cat";
                    error(execute_command(command));
                } else {
                    error("\n" + current_errors_file_source);
                }
            }
            memory_pool->ReturnTicket(ticket);
            done();
        });
    });
}

void BaselineTestRunner::Run() {
  RunTestSuites(option.with_timeout);
  PrintStats();
}

static int write_data(char *data, size_t size, size_t nmemb, std::string *userp) {
  return size * nmemb;
}

void
BaselineTestRunner::DefineHttpTests() {
    domain("HTTP");
    VisistTestsByPath("src/program", [&](const TestCase &test_case) {
        if (option.folder && *option.folder != test_case.folder) {
            return;
        }
        if (option.test && *option.test != test_case.name) {
            return;
        }
        test(test_case.name, [=](Test *test, std::function<void()> success, std::function<void(std::string error)> error) -> void {
            CURL *curl;
            CURLcode code;
            ServerTestScanner server_test_scanner(test_case.source.c_str(), test_case.source.size());
            server_test_scanner.Scan();
            auto start = std::chrono::steady_clock::now();
            for (auto &backend : server_test_scanner.backends) {
                pipe(&backend->pipe_fd[0]);
                backend->child_pid = fork();

                if (backend->child_pid == -1) {
                    printf("Could not fork child process. %s\n", strerror(errno));
                    return;
                }

                if (backend->child_pid == 0) {
                    close(backend->pipe_fd[1]);
                    auto p = resolve_paths(root_dir(), "src/test/test_server/bin/test");
                    const char *command = p.c_str();
                    char fd_flag[10];
                    sprintf(fd_flag, "--fd=%d", backend->pipe_fd[0]);
                    Json::Value root = Json::arrayValue;
                    for (const auto &graphql_response : backend->graphql_responses) {
                        Json::Value response_json;
                        response_json["assertGraphqlField"] = ToCharArray(graphql_response.field);
                        response_json["assertGraphqlLocation"] = "payload";
                        response_json["body"] = ToCharArray(graphql_response.body);
                        root.append(response_json);
                    }
                    Json::StyledWriter writer;
                    int r = execl(command,
                        "test",
                        "--responses", writer.write(root).c_str(),
                        fd_flag,
                        (char*)NULL
                    );
                    if (r == -1) {
                        std::cout << command << std::endl;
                        printf("Could not execute script. %s\n", strerror(errno));
                    }
                    return;
                }

                do {
                    close(backend->pipe_fd[0]);
                    curl = curl_easy_init();
                    curl_global_init(CURL_GLOBAL_ALL);

                    if (curl) {
                        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
                        code = curl_easy_setopt(curl, CURLOPT_URL, "https://localhost:8000");
                        if (code != CURLE_OK) {
                            fprintf(stderr, "Failed to set URL [%s]\n", errorBuffer);
                            return;
                        }
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
                        curl_slist *slist1 = nullptr;
                        slist1 = curl_slist_append(slist1, "Content-Type: application/json");
                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist1);
                        const char* body = ToCharArray(server_test_scanner.request.body);
                        code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
                        if (code != CURLE_OK) {
                            fprintf(stderr, "Failed to set body [%s]\n", errorBuffer);
                            return;
                        }

                        code = curl_easy_perform(curl);
                        if (code != CURLE_OK) {
                            auto end = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                            if (duration < 5000) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                                curl_easy_cleanup(curl);
                                curl_global_cleanup();
                                continue;
                            }
                            else {
                                fprintf(stderr, "curl_easy_perform(): [%s]\n", errorBuffer);
                                return;
                            }
                        }
                        curl_easy_cleanup(curl);
                        curl_global_cleanup();
                        break;
                    }
                }
                while (true);
            }
            success();
            for (auto &backend : server_test_scanner.backends) {
                kill(backend->child_pid, SIGTERM);
            }
        });
    });
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

std::string
BaselineTestRunner::line_number()
{
    return line_number(current_line_);
}


std::string
BaselineTestRunner::line_number(std::size_t line)
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

void
BaselineTestRunner::append_mutation_chunk(
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
                tw.write(line_number(++current_line_));
            }
            else {
                tw.write(line_number(current_line_ + n));
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
                tw.write(line_number(++current_line_));
            }
            else {
                tw.write(line_number(current_line_ + n));
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

std::tuple<std::string, bool>
BaselineTestRunner::append_all_lines(const std::string &chunk, TextWriter &tw)
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


int BaselineTestRunner::StartServer(int fd) {
    uv_loop_t* loop = uv_default_loop();
    HttpServer server(loop);
    server.fd = fd;
    server.Listen("0.0.0.0", 8000);
    return uv_run(loop, UV_RUN_DEFAULT);
}

}

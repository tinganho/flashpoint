#include <iostream>
#include <program/http_server.h>
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
using namespace flashpoint::program;

namespace flashpoint::test {
    boost::filesystem::path root_folder(root_path());
    boost::filesystem::path test_folder(root_folder / "src/program/tests/");

    BaselineTestRunner::BaselineTestRunner(uv_loop_t* loop):
        loop(loop)
    {
    }


    void handle_sighup(int signum)
    { }

    void BaselineTestRunner::visit_tests(std::function<void(const char*, boost::filesystem::path, std::string)> callback)
    {
        std::vector<std::string> tests = find_files(root_path() + "src/program/tests/cases/*");
        for (const auto& t : tests) {
            const char* content = read_file(t);
            std::vector<std::string> paths = split_string(replace_string(t, (test_folder / "cases").string(), ""), '/');
            std::string file = paths.back();
            std::vector<std::string> folder_paths(paths.begin(), paths.end() - 1);
            std::stringstream ss;
            for (int i = 0; i < folder_paths.size(); i++) {
                if (i != 0) {
                    ss << "/";
                }
                ss << folder_paths[i];
            }
            std::string folder = ss.str();
            callback(content, folder, file);
        }
    }

    void BaselineTestRunner::run() {
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
            run_tests();
            kill(parent_pid, SIGHUP);
        }
    }

    int BaselineTestRunner::run_tests()
    {
        domain("Baselines");
        visit_tests([&](const char* content, boost::filesystem::path folder, boost::filesystem::path file) {
            test(file.string(), [=](Test* test, std::function<void()> success, std::function<void(std::string error)> error) {
                boost::filesystem::path current_folder(test_folder / "currents");
                boost::filesystem::path reference_folder(test_folder / "references");
                boost::filesystem::path current_file = current_folder / file;
                boost::filesystem::path reference_file = reference_folder / file;

                const char* reference = "";
                if (boost::filesystem::exists(reference_file)) {
                    reference = read_file(reference_file);
                }
                TcpClientRaw tcp_client;
                tcp_client._connect("0.0.0.0", 8000);
                tcp_client.send_message(content);
                const char* current = tcp_client.recieve_message();
                if (!path_exists(current_folder)) {
                    create_folder(current_folder);
                }
                write_file(current_file, current);
                if (strcmp(current, reference) != 0) {
                    std::stringstream ss;
                    diff_match_patch<std::string> diff;
                    std::string r(reference);
                    std::string c(current);
                    auto diffs = diff.diff_main(r, c);
                    diff_match_patch<std::string>::diff_cleanupSemantic(diffs);
                    for (const auto& d : diffs) {
                        if (d.operation == diff_match_patch<std::string>::Operation::DELETE) {
                            ss << "\e[31m" << d.text << "\e[0m";
                        }
                        else if (d.operation == diff_match_patch<std::string>::Operation::INSERT) {
                            ss << "\e[32m" << d.text << "\e[0m";
                        }
                        else {
                            ss << d.text;
                        }
                    }
                    return error("\n\e[0m" + ss.str());
                }
                success();
            });
        });
        run_test_suites();
        print_result();
    }

    int BaselineTestRunner::start_server() {
        HttpServer server(loop);
        server.listen("0.0.0.0", 8000);
    }
}

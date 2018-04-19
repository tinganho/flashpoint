#include <iostream>
#include <test/baseline_test_runner.h>
#include <test/test_definition.h>
#include <lib/tcp_client.h>
#include <lib/utils.h>

using namespace flashpoint::lib;

namespace flashpoint::test {
    BaselineTestRunner::BaselineTestRunner()
    {
    }

    void BaselineTestRunner::visit_tests(std::function<void(const std::string&)> callback)
    {
        std::vector<std::string> tests = find_files(root_path() + "src/program/tests/cases/*");
        for (const auto& t : tests) {
            std::string content = read_file(t);
            callback(content);
        }
    }

    void BaselineTestRunner::run() {
        domain("Baselines");
        visit_tests([&](const std::string& file) {
            TcpClient tcp_client("127.0.0.1", 8000);
            tcp_client._connect();
        });
        run_tests();
    }
}


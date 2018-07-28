#include <iostream>
#include <test/baseline_test_runner.h>
#include <lib/command.h>
#include <signal.h> // signals
#include <future>
#include <uv.h>

using namespace flashpoint::lib;
using namespace flashpoint::test;

int main(int argc, char* argv[]) {
    BaselineTestRunner test_runner;
    std::vector<CommandDefinition> commands = {
        { "", "default", "",
            {
                { "test", "t", "Run specific test", true, false, "" } ,
                { "folder", "", "Run specific folder", true, false, "" }
            }
        },
        { "", "accept", "Accept baselines",
            {
                { "test", "t", "Accept specific test", true, false, "" },
                { "folder", "f", "Accept specific folder", true, false, "" },
                { "all", "a", "Accept all tests", true, false, "" },
            }
        }
    };
    Command command(argc, argv, commands);
    if (command.errors.size() > 0) {
        for (const auto& error : command.errors) {
            std::cerr << error << std::endl;
        }
        return 1;
    };
    RunOption run_option;
    if (command.has_flag("test")) {
        run_option.test = std::string(command.get_flag_value("test"));
    }
    else if (command.has_flag("folder")) {
        run_option.folder = std::string(command.get_flag_value("folder"));
    }
    if (command.is("accept")) {
        test_runner.accept_graphql_tests(run_option);
    }
    else {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            std::cerr << "An error occurred when forking process" << std::endl;
            return 1;
        }
        if (child_pid == 0) {
            test_runner.start_server();
        }
        else {
            test_runner.define_graphql_tests(run_option);
            test_runner.define_http_tests(run_option);
            test_runner.run(run_option);

            kill(child_pid, SIGTERM);
        }
    }
//    if (argc > 1) {
//        if(strcmp(argv[1], "--no-server") == 0) {
//            test_runner.run_http_tests(run_option);
//        }
//    }
//    else {
//        test_runner.run_http_tests_with_server(run_option);
//    }

    return 0;
}

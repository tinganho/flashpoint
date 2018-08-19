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
                { "folder", "", "Run specific folder", true, false, "" },
                { "use-external-server", "", "Use external server", false, false, "" },
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
    RunOption run_option = { nullptr, nullptr };

    if (command.has_flag("test")) {
        run_option.test = new std::string(command.get_flag_value("test"));
    }
    else if (command.has_flag("folder")) {
        run_option.folder = new std::string(command.get_flag_value("folder"));
    }
    if (command.is("accept")) {
        test_runner.AcceptGraphQlTests(run_option);
    }
    else {
        if (command.has_flag("use-external-server")) {
            test_runner.DefineGraphQlTests(run_option);
            test_runner.DefineHttpTests(run_option);
            test_runner.Run(run_option);
            return 0;
        }
        pid_t child_pid = fork();
        if (child_pid == -1) {
            std::cerr << "An error occurred when forking process" << std::endl;
            return 1;
        }
        if (child_pid == 0) {
            if (!command.has_flag("start-server")) {
                test_runner.StartServer();
            }
        }
        else {
            test_runner.DefineGraphQlTests(run_option);
//            test_runner.DefineHttpTests(run_option);
            test_runner.Run(run_option);

            kill(child_pid, SIGTERM);
        }
    }

    return 0;
}

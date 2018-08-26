#include <iostream>
#include <test/baseline_test_runner.h>
#include <lib/command.h>
#include <signal.h> // signals
#include <future>
#include <uv.h>

using namespace flashpoint::lib;
using namespace flashpoint::test;

int main(int argc, char* argv[]) {
    std::vector<CommandDefinition> commands = {
        { "", "default", "",
            {
                { "test", "t", "Run specific test", true, false, "" } ,
                { "folder", "", "Run specific folder", true, false, "" },
                { "only-http-tests", "", "Run only HTTP tests", false, false, "" },
                { "only-graphql-tests", "", "Run only GraphQL tests", false, false, "" },
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
    BaselineTestRunnerOption run_option = { nullptr, nullptr };
    if (command.HasFlag("test")) {
        run_option.test = new std::string(command.get_flag_value("test"));
    }
    else if (command.HasFlag("folder")) {
        run_option.folder = new std::string(command.get_flag_value("folder"));
    }
    if (command.HasFlag("use-external-server")) {
        run_option.with_timeout = false;
    }
    else {
        run_option.with_timeout = true;
    }
    BaselineTestRunner test_runner(run_option);
    if (command.Is("accept")) {
        test_runner.AcceptGraphQlTests();
    }
    else {
        if (!command.HasFlag("use-external-server")) {
            test_runner.DefineGraphQlTests();
            test_runner.DefineHttpTests();
            test_runner.Run();
            return EXIT_SUCCESS;
        }
        int pipefds[2];
        if (pipe(pipefds)) {
            fprintf (stderr, "Pipe failed. %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        pid_t child_pid = fork();
        if (child_pid == -1) {
            fprintf(stderr, "Could not fork process.  %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        if (child_pid == 0) {
            if (!command.HasFlag("use-external-server")) {
                close(pipefds[1]);
                test_runner.StartServer(pipefds[0]);
            }
        }
        else {
            close(pipefds[0]);
            write(pipefds[1], "h", 1);
            if (command.HasFlag("only-http-tests")) {
                test_runner.DefineHttpTests();
            }
            else if (command.HasFlag("only-graphql-tests")) {
                test_runner.DefineGraphQlTests();
            }
            else {
                test_runner.DefineGraphQlTests();
                test_runner.DefineHttpTests();
            }
            test_runner.Run();

            kill(child_pid, SIGTERM);
        }
    }

    return 0;
}

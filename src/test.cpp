#include <iostream>
#include <test/baseline_test_runner.h>
#include <lib/command.h>
#include <future>
#include <uv.h>

using namespace flashpoint::lib;
using namespace flashpoint::test;

int main(int argc, char* argv[]) {
    uv_loop_t* loop = uv_default_loop();
    BaselineTestRunner test_runner(loop);
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
        test_runner.run_graphql_tests(run_option);
    }
//    if (argc > 1) {
//        if(strcmp(argv[1], "--no-server") == 0) {
//            test_runner.run_http_tests();
//        }
//    }
//    else {
//        test_runner.run_http_tests_with_server();
//    }

//        std::tuple<int, const char*> t = std::make_tuple(1, "wefew");
//        int n = std::get<0>(t);
    return 0;
}

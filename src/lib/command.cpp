#include "command.h"
#include <cstring>
#include <iostream>

namespace flashpoint::lib {

Command::Command(int argc, char **argv, const std::vector<CommandDefinition>& commands):
    commands(commands)
{
    bool found_command = false;
    if (argc > 0) {
        for (const auto& c : commands) {
            const char* com = argv[1];
            if (com != nullptr && com[0] != '-' && strcmp(com, c.name) == 0) {
                command = c;
                found_command = true;
            }
        }
    }
    if (!found_command) {
        command = commands.at(0);
    }
    Flag flag_awaiting_value;
    bool flag_is_awaiting_value = false;
    for_each_arg(argc, argv, [&](char* arg) {
        if (arg[0] == '-') {
            char* flag = arg + 2;
            char* value;
            bool has_equal_value = false;
            auto size = strlen(arg);
            for (std::size_t i = 0; i < size; i++) {
                if (arg[i] == '=') {
                    value = new char[size - i];
                    strcpy(value, arg + i + 1);
                    value[size - i - 1] = '\0';
                    has_equal_value = true;
                    strncpy(flag, arg + 2, i - 2);
                    flag[i - 2] = '\0';
                    break;
                }
            }

            bool defined_in_command = false;
            for (const auto& command_flag : command.flags) {
                if (strcmp(command_flag.name, flag) == 0) {
                    defined_in_command = true;
                    Flag flag {
                        command_flag.name,
                        command_flag.description,
                        command_flag.alias,
                        command_flag.has_value
                    };
                    if (command_flag.has_value) {
                        if (has_equal_value) {
                            flag.value = value;
                            flags.push_back(flag);
                        }
                        else {
                            flag_awaiting_value = flag;
                            flag_is_awaiting_value = true;
                        }
                    }
                    else {
                        flags.push_back(flag);
                    }
                    break;
                }
            }
            if (!defined_in_command) {
                warnings.emplace_back((std::string("Unsupported flag '--") + arg + "'.").c_str());
            }
        }
        else if (flag_is_awaiting_value) {
            flag_awaiting_value.value = arg;
            flags.push_back(flag_awaiting_value);
        }
    });
}

const char*
Command::get_flag_value(const char* flag)
{
    for (const auto& f : flags) {
        if (strcmp(f.name, flag) == 0) {
            return f.value;
        }
    }
    return "";
}

bool
Command::Is(const char *c)
{
    return strcmp(command.name, c) == 0;
}

bool
Command::HasFlag(const char *flag)
{
    for (const auto f : flags) {
        if (strcmp(f.name, flag) == 0) {
            return true;
        }
    }
    return false;
}

void Command::for_each_arg(int argc, char** argv, std::function <void(char *)> callback)
{
    for (int arg_index = 1; arg_index < argc; arg_index++) {
        callback(argv[arg_index]);
    }
}

}
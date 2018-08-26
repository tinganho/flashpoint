#ifndef FLASHPOINT_COMMAND_PARSER_H
#define FLASHPOINT_COMMAND_PARSER_H

#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace flashpoint::lib {

struct FlagDefinition {
    const char* name;
    const char* description;
    const char* alias;
    bool has_value;
    bool has_default_value;
    const char* default_value;
};

struct Flag {
    const char* name;
    const char* description;
    const char* alias;
    bool has_value;
    const char* value;
};

struct CommandDefinition {
    const char* _namespace;
    const char* name;
    const char* description;
    std::vector<FlagDefinition> flags;
};

class Command {
public:

    Command(int argc, char** argv, const std::vector<CommandDefinition>& commands);

    std::vector<Flag>
    flags;

    const char*
    description;

    const char*
    _namespace;

    const char*
    current_command;

    std::vector<const char*>
    warnings;

    std::vector<const char*>
    errors;

    bool
    Is(const char *command);

    bool
    HasFlag(const char *flag);

    const char*
    get_flag_value(const char* flag);

private:

    const std::vector<CommandDefinition>&
    commands;

    CommandDefinition
    command;

    std::unique_ptr<FlagDefinition>
    flag_which_awaits_value;

    void
    for_each_arg(int argc, char** argv, std::function<void (char*)> callback);
};

}

#endif //FLASHPOINT_COMMAND_PARSER_H

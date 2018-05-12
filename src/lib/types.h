
#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace flashpoint::lib {

    enum class OridinalCategory {
        None =      0,
        Zero =      1 << 0,
        One =       1 << 1,
        Two =       1 << 2,
        Few =       1 << 3,
        Many =      1 << 4,
        Other =     1 << 5,
        Specified = 1 << 6,
    };

    enum class CommandKind {
        None,
        Init,
        Sync,
        Log,
        Set,
        Extension_RunTests,
        Extension_AcceptBaselines,
    };

    enum class FlagKind {
        None,
        Help,
        Version,
        Language,
        Id,
        Value,
        RootDir,
        Grep,
        NoServer,
        StartLine,
        Test,
    };

    struct Span {
        unsigned int start;
        unsigned int end;
    };

    struct SpanLocation {
        unsigned long long line;
        unsigned long long column;
        unsigned long long length;
        unsigned long long position;
    };

    struct Location {
        unsigned int line;
        unsigned int column;
    };

    struct Argument {
        std::string name;
        std::string description;

        Argument(std::string _name, std::string _description):
                name(std::move(_name)),
                description(std::move(_description))
        { }
    };
} // flash::lib

#endif // TYPES_H

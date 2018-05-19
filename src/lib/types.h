
#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace flashpoint::lib {

    struct Location {
        std::size_t line;
        std::size_t column;
        std::size_t length;
        bool end_of_source;
    };
} // flash::lib

#endif // TYPES_H

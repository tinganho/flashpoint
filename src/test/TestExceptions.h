#ifndef FLASHPOINT_TESTEXCEPTIONS_H
#define FLASHPOINT_TESTEXCEPTIONS_H

#include <stdexcept>
#include <exception>

class ValidationException : public std::runtime_error
{
public:
    explicit ValidationException(const std::string &message) noexcept;
};

#endif //FLASHPOINT_TESTEXCEPTIONS_H

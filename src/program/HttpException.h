#ifndef FLASHPOINT_HTTP_EXCEPTIONS_H
#define FLASHPOINT_HTTP_EXCEPTIONS_H

#include <stdexcept>
#include <exception>

class HttpParsingError : public std::runtime_error
{
public:
    explicit HttpParsingError(const std::string &message) noexcept;
};

class BufferOverflowException : public std::exception
{
public:
    explicit BufferOverflowException() noexcept;
};

class UriTooLongException : public std::exception
{
public:
    explicit UriTooLongException() noexcept;
};

class UnexpectedTokenException : public std::runtime_error
{
public:
    explicit UnexpectedTokenException(const std::string &message) noexcept;
};

class InvalidGraphQlQueryException : public std::exception
{
public:
    explicit InvalidGraphQlQueryException() noexcept;
};
#endif //FLASHPOINT_HTTP_EXCEPTIONS_H

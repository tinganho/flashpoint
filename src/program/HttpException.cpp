#include "HttpException.h"

HttpParsingError::HttpParsingError(const std::string &message) noexcept
    : std::runtime_error(message)
{ }

BufferOverflowException::BufferOverflowException() noexcept
    : std::exception()
{ }

UriTooLongException::UriTooLongException() noexcept
    : std::exception()
{ }

UnexpectedTokenException::UnexpectedTokenException(const std::string &message) noexcept
    : std::runtime_error(message)
{ }

InvalidGraphQlQueryException::InvalidGraphQlQueryException() noexcept
    : std::exception()
{ }
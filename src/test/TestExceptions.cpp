//
// Created by Tingan Ho on 2018-08-28.
//

#include "TestExceptions.h"

ValidationException::ValidationException(const std::string &message) noexcept
    : std::runtime_error(message)
{ }

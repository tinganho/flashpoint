#ifndef FLASHPOINT_TOKEN_VALUE_H
#define FLASHPOINT_TOKEN_VALUE_H

#include <vector>
#include <string>

namespace flashpoint {

struct TextSpan {
    const char *value;
    std::size_t length;

    std::string to_string() const {
        return std::string(c_str());
    }

    char* c_str() const {
        char* str = new char[length + 1];
        strncpy(str, value, length);
        str[length] = '\0';
        return str;
    }
};

struct TextSpanComparer {
    bool operator()(const TextSpan& a, const TextSpan& b) const
    {
        auto min = std::min(a.length, b.length);
        for (std::size_t i = 0; i < min; i++) {
            if (a.value[i] > b.value[i]) {
                return true;
            }
            else if (a.value[i] < b.value[i]) {
                return false;
            }
            else {
                continue;
            }
        }
        return a.length > b.length;
    }
};

char *
ToCharArray(const TextSpan* text_span)
{
    char* str = new char[text_span->length + 1];
    str[0] = '\0';
    strncpy(str, text_span->value, text_span->length);
    return str;
}

char *
ToCharArray(const std::vector<TextSpan*> &token_value_list)
{
    std::size_t size = 0;
    for (const auto &token_value : token_value_list) {
        size += token_value->length;
    }
    auto body = new char[size];
    body[0] = '\0';
    for (const auto &token_value : token_value_list) {
        strncat(body, token_value->value, token_value->length);
    }
    return body;
}

}
#endif //FLASHPOINT_TOKEN_VALUE_H

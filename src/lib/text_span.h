#ifndef FLASHPOINT_TOKEN_VALUE_H
#define FLASHPOINT_TOKEN_VALUE_H

#include <vector>
#include <string>

namespace flashpoint {

struct TextSpan {
    const char *value;
    std::size_t length;

    TextSpan(const char* text):
        value(text),
        length(strlen(text) - 1)
    { }

    TextSpan(const char* text, std::size_t length):
        value(text),
        length(length)
    { }
};

struct TextSpan_Test {
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
    bool operator()(const TextSpan* a, const TextSpan* b) const
    {
        auto min = std::min(a->length, b->length);
        for (std::size_t i = 0; i < min; i++) {
            if (a->value[i] > b->value[i]) {
                return true;
            }
            else if (a->value[i] < b->value[i]) {
                return false;
            }
            else {
                continue;
            }
        }
        return a->length > b->length;
    }
};

char *
ToCharArray(const TextSpan* text_span);

char *
ToCharArray(const std::vector<TextSpan*>& token_value_list);

}
#endif //FLASHPOINT_TOKEN_VALUE_H

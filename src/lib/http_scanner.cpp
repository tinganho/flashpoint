//
// Created by Tingan Ho on 2018-04-08.
//

#include <iostream>
#include <tuple>
#include "utils.h"
#include "http_scanner.h"

namespace flash::lib {

    HttpScanner::HttpScanner(char* text, unsigned int size):
        position(0),
        start_position(0),
        text(text),
        size(size)
    { }

    char HttpScanner::current_char()
    {
        return text[position];
    }

    HttpMethod HttpScanner::scan_method()
    {
        HttpMethod token = HttpMethod::None;
        switch (current_char())
        {
            case Character::C:
                position += 7;
                token = HttpMethod::Connect;
                break;
            case Character::D:
                position += 6;
                token = HttpMethod::Delete;
                break;
            case Character::G:
                position += 3;
                token = HttpMethod::Get;
                break;
            case Character::H:
                position += 4;
                token = HttpMethod::Head;
                break;
            case Character::O:
                position += 7;
                token = HttpMethod::Options;
                break;
            case Character::P:
                increment_position();
                switch (current_char()) {
                    case Character::A:
                        position += 4;
                        token = HttpMethod::Patch;
                        break;
                    case Character::O:
                        position += 3;
                        token = HttpMethod::Post;
                        break;
                    case Character::U:
                        position += 2;
                        token = HttpMethod::Put;
                        break;
                }
                break;
            case Character::T:
                position += 5;
                token = HttpMethod::Trace;
                break;
        }
        if (position >= size) {
            throw std::logic_error("Invalid request.");
        }
        return token;
    }

    char* HttpScanner::scan_absolute_path()
    {
        set_token_start_position();
        scan_expected(Character::Slash);
        char ch = current_char();
        while (position < size && (is_pchar(ch) || ch == Character::Slash)) {
            increment_position();
        }
        return get_token_value();
    }

    char* HttpScanner::scan_query()
    {
        set_token_start_position();
        if (next_char_is(Character::Question)) {
            char ch = current_char();
            while (position < size && (is_pchar(ch) || ch == Character::Slash || ch == Character::Question)) {
                increment_position();
                ch = current_char();
            }
        }
        return get_token_value();
    }

    RequestLineToken HttpScanner::scan_http_version()
    {
        scan_expected(Character::H);
        position += 7;
        char ch = text[position];
        return RequestLineToken::HttpVersion1_1;
    }

    HttpHeader HttpScanner::scan_header()
    {
        set_token_start_position();
        if (!is_header_field_start(current_char())) {
            if (scan_optional(Character::CarriageReturn)) {
                scan_expected(Character::LineFeed);
                return HttpHeader::End;
            }
            throw std::logic_error("Parse error in the header");
        }
        while (position < size && is_header_field_part(current_char())) {
            increment_position();
        }
        HttpHeader header = get_header(get_token_value());
        scan_expected(Character::Colon);
        scan_optional(Character::Space);
        set_token_start_position();
        while (position < size && is_vchar(current_char())) {
            increment_position();
        }
        current_header = get_token_value();
        scan_optional(Character::Space);
        scan_expected(Character::CarriageReturn);
        scan_expected(Character::LineFeed);
        return header;
    }

    char* HttpScanner::get_header_value()
    {
        return current_header;
    }

    HttpHeader HttpScanner::get_header(char *ch)
    {
        auto it = string_to_token.find(ch);
        if (it != string_to_token.end())
        {
            return it->second;
        }

        return HttpHeader::Unknown;
    }

    bool HttpScanner::is_header_field_start(char ch)
    {
        return (ch >= Character::A && ch <= Character::Z);
    }

    bool HttpScanner::is_header_field_part(char ch)
    {
        return (ch >= Character::a && ch <= Character::z) ||
               (ch >= Character::A && ch <= Character::Z) ||
               ch == Character::Dash;
    }

    bool HttpScanner::is_method_part(char ch)
    {
        return (ch >= Character::A && ch <= Character::Z);
    }

    void HttpScanner::scan_request_target()
    {
        while (position < size)
        {
            char ch = current_char();
            increment_position();

            if (ch == Character::Space) {
                return;
            }
            if (ch == Character::Slash) {
                continue;
            }
            if (is_pchar(ch)) {
                continue;
            }
            if (ch == Character::Question) {
                return;
            }
        }

        throw std::logic_error("Should not reach here.");
    }

    bool HttpScanner::is_pchar(char ch)
    {
        if (is_unreserverd_char(ch)) {
            return true;
        }
        if (is_sub_delimiter(ch)) {
            return true;
        }
        if (ch == Character::Colon) {
            return true;
        }
        if (ch == Character::At) {
            return true;
        }
        if (ch == Character::Percent) {
            increment_position();
            if (!ishexnumber(current_char())) {
                throw std::logic_error("Expected hex number.");
            }
            increment_position();
            if (!ishexnumber(current_char())) {
                throw std::logic_error("Expected hex number.");
            }
            return true;
        }
        return false;
    }

    bool HttpScanner::is_unreserverd_char(char ch)
    {
        if (isalnum(ch)) {
            return true;
        }

        switch (ch) {
            case Minus:
            case Dot:
            case Underscore:
            case Tilde:
                return true;
        }
        return false;
    }

    bool HttpScanner::is_sub_delimiter(char ch)
    {
        switch (ch) {
            case Exclamation:
            case Dollar:
            case Ampersand:
            case SingleQuote:
            case OpenParen:
            case CloseParen:
            case Asterisk:
            case Plus:
            case Comma:
            case Semicolon:
            case Equal:
                return true;
        }
        return false;
    }

    bool HttpScanner::is_vchar(char ch)
    {
        if (ch >= Exclamation && ch <= Tilde) {
            return true;
        }
        return false;
    }


    void HttpScanner::increment_position()
    {
        if (position >= size) {
            throw std::logic_error("You cannot increment more than max size.");
        }
        position++;
    }

    bool HttpScanner::scan_optional(char ch)
    {
        save();
        if (current_char() == ch) {
            increment_position();
            return true;
        }
        revert();
        return false;
    }

    bool HttpScanner::next_char_is(char ch) {
        return scan_optional(ch);
    }

    void HttpScanner::scan_expected(char ch)
    {
        if (current_char() == ch) {
            increment_position();
            return;
        }
        throw std::logic_error(std::string("Expected character character '") + ch  + "'");
    }


    char HttpScanner::peek_next_char()
    {
        char ch;
        save();
        increment_position();
        ch = current_char();
        revert();
        return ch;
    }

    void HttpScanner::save()
    {
        SavedTextCursor saved_text_cursor {
            position,
            start_position,
            end_position,
        };
        saved_text_cursors.push(saved_text_cursor);
    }

    void HttpScanner::revert()
    {
        const SavedTextCursor& saved_text_cursor = saved_text_cursors.top();
        position = saved_text_cursor.position;
        start_position = saved_text_cursor.start_position;
        end_position = saved_text_cursor.end_position;
        saved_text_cursors.pop();
    }


    void HttpScanner::set_token_start_position()
    {
        start_position = position;
    }

    void HttpScanner::scan_rest_of_line()
    {
        while (position < size && current_char() != Character::CarriageReturn) {
            increment_position();
        }
        increment_position();
        increment_position();
    }

    char* HttpScanner::get_token_value() const
    {
        int size = position - start_position + 1;
        char* str = (char*)malloc(size);
        std::strncpy(str, text + start_position, size);
        str[size - 1] = '\0';
        return str;
    }
}
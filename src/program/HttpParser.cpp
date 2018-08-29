#include <iostream>
#include <vector>
#include <program/HttpParser.h>
#include <lib/character.h>
#include "HttpException.h"

using namespace flashpoint::lib;

namespace flashpoint {

HttpParser::HttpParser()
{ }

void
HttpParser::ParseRequest(char *buffer, std::size_t read_length)
{
    AddBuffer(buffer, read_length);
    while (location != ParsingLocation::End) {
        switch (location) {
            case ParsingLocation::StartLine:
                try {
                    ParseRequestLine();
                }
                catch (BufferOverflowException& ex) {
                    throw UriTooLongException();
                }
                break;
            default:
                ParseDefaultLocations();
        }
    }
}

void
HttpParser::ParseResponse(char *buffer, std::size_t read_length)
{
    AddBuffer(buffer, read_length);
    while (location != ParsingLocation::End) {
        switch (location) {
            case ParsingLocation::StartLine:
                try {
                    ParseStatusLine();
                }
                catch (BufferOverflowException& ex) {
                    throw UriTooLongException();
                }
                break;
            default:
                ParseDefaultLocations();
        }
    }
}

bool
HttpParser::IsFinished()
{
    return location == ParsingLocation::End;
}

void
HttpParser::ParseDefaultLocations()
{
    switch (location) {
        case ParsingLocation::HeaderName:
            ParseHeaderName();
            break;
        case ParsingLocation::HeaderValue:
            ParseHeaderValue();
            break;
        case ParsingLocation::Body:
            ParseBody();
            break;
        default:
            throw std::logic_error("Should not reach here.");
    }
}

void
HttpParser::ParseBody()
{
    body.push_back(ScanBody());
}

void
HttpParser::ParseStatusLine()
{
    ScanHttpVersion();
    status_code = ScanPositiveInteger();
    reason_phrase = ScanReasonPhrase();
    location = ParsingLocation::HeaderName;
}

void
HttpParser::ParseRequestLine() {
    method = ScanMethod();
    ScanExpected(Character::Space);
    path = ScanAbsolutePath();
    query = ScanQuery();
    ScanExpected(Character::Space);
    ScanHttpVersion();
    ScanExpected(Character::CarriageReturn);
    ScanExpected(Character::NewLine);
    location = ParsingLocation::HeaderName;
}

void HttpParser::AddBuffer(char *buffer, std::size_t read_length)
{
    buffer_position = 0;
    current_buffer_text = buffer;
    current_buffer_read_length = read_length;
}

char
HttpParser::GetCurrentChar()
{
    return current_buffer_text[buffer_position];
}

HttpMethod
HttpParser::ScanMethod()
{
    HttpMethod token = HttpMethod::None;
    switch (GetCurrentChar()) {
        case Character::C:
            buffer_position += 7;
            token = HttpMethod::Connect;
            break;
        case Character::D:
            buffer_position += 6;
            token = HttpMethod::Delete;
            break;
        case Character::G:
            buffer_position += 3;
            token = HttpMethod::Get;
            break;
        case Character::H:
            buffer_position += 4;
            token = HttpMethod::Head;
            break;
        case Character::O:
            buffer_position += 7;
            token = HttpMethod::Options;
            break;
        case Character::P:
            IncrementPosition();
            switch (GetCurrentChar()) {
                case Character::A:
                    buffer_position += 4;
                    token = HttpMethod::Patch;
                    break;
                case Character::O:
                    buffer_position += 3;
                    token = HttpMethod::Post;
                    break;
                case Character::U:
                    buffer_position += 2;
                    token = HttpMethod::Put;
                    break;
            }
            break;
        case Character::T:
            buffer_position += 5;
            token = HttpMethod::Trace;
            break;
    }
    if (buffer_position >= current_buffer_read_length) {
        throw std::logic_error("Invalid request.");
    }
    return token;
}

TokenValue
HttpParser::ScanBody()
{
    SetTokenStartPosition();
    while (true) {
        if (has_content_length && body_length >= content_length) {
            location = ParsingLocation::End;
            break;
        }
        if (has_content_length) {
            auto rest = current_buffer_read_length - buffer_position;
            body_length += rest;
            IncrementPosition(rest);
        }
    }
    return GetTokenValue();
}

TokenValue
HttpParser::ScanAbsolutePath()
{
    SetTokenStartPosition();
    ScanExpected(Character::Slash);
    char ch = GetCurrentChar();
    while (buffer_position < current_buffer_read_length && (IsPchar(ch) || ch == Character::Slash)) {
        IncrementPosition();
        ch = GetCurrentChar();
    }
    return GetTokenValue();
}

TokenValue
HttpParser::ScanQuery()
{
    SetTokenStartPosition();
    if (NextCharIs(Character::Question)) {
        char ch = GetCurrentChar();
        while (buffer_position < current_buffer_read_length && (IsPchar(ch) || ch == Character::Slash || ch == Character::Question)) {
            IncrementPosition();
            ch = GetCurrentChar();
        }
    }
    return GetTokenValue();
}

StartLineToken
HttpParser::ScanHttpVersion()
{
    ScanExpected(Character::H);
    buffer_position += 7;
    return StartLineToken::HttpVersion1_1;
}

unsigned int
HttpParser::ScanPositiveInteger()
{
    while (IsDigit(GetCurrentChar())) {
        IncrementPosition();
    }
    auto token_value = GetTokenValue();
    return static_cast<unsigned int>(std::atoi(token_value.value));
}

bool
HttpParser::IsDigit(char ch)
{
    return ch >= Character::_0 && ch <= Character::_9;
}

TokenValue
HttpParser::ScanReasonPhrase()
{
    while (true) {
        auto ch = GetCurrentChar();
        if (ch == Character::NewLine) {
            break;
        }
        if (ch == Character::Space) {
            continue;
        }
        if (ch == Character::HorizontalTab) {
            continue;
        }
        if (IsVchar(ch)) {
            continue;
        }
        if (IsObsText(ch)) {
            continue;
        }
        throw HttpParsingError("Could not parse reason phrase on status line.");
    }
    return GetTokenValue();
}

TokenValue*
HttpParser::GetHeader(const char* name)
{
    auto header_it = headers.find(TokenValue { name, strlen(name) });
    if (header_it != headers.end()) {
        return &header_it->second;
    }
    return nullptr;
}

std::size_t
HttpParser::ToUint(TokenValue* token_value)
{
    std::size_t integer_value = 0;
    auto value = token_value->value;
    auto length = token_value->length;
    for (std::size_t i = 0; i < length; i++) {
        switch (value[length - i - 1]) {
            case '0':
                continue;
            case '1':
                integer_value += 1 * (std::size_t)std::pow(10, i);
                break;
            case '2':
                integer_value += 2 * (std::size_t)std::pow(10, i);
                break;
            case '3':
                integer_value += 3 * (std::size_t)std::pow(10, i);
                break;
            case '4':
                integer_value += 4 * (std::size_t)std::pow(10, i);
                break;
            case '5':
                integer_value += 5 * (std::size_t)std::pow(10, i);
                break;
            case '6':
                integer_value += 6 * (std::size_t)std::pow(10, i);
                break;
            case '7':
                integer_value += 7 * (std::size_t)std::pow(10, i);
                break;
            case '8':
                integer_value += 8 * (std::size_t)std::pow(10, i);
                break;
            case '9':
                integer_value += 9 * (std::size_t)std::pow(10, i);
                break;
            default:
                throw HttpParsingError("Could not parse integer.");
        }
    }
    return integer_value;
}

void
HttpParser::ParseHeaderName()
{
    SetTokenStartPosition();
    if (!IsHeaderFieldStart(GetCurrentChar())) {
        if (ScanOptional(Character::CarriageReturn)) {
            ScanExpected(Character::NewLine);
            location = ParsingLocation::Body;
            auto content_length = GetHeader("content-length");
            if (content_length != nullptr) {
                has_content_length = true;
                this->content_length = ToUint(content_length);
            }
            return;
        }
    }
    while (buffer_position < current_buffer_read_length && IsHeaderFieldPart(GetCurrentChar())) {
        IncrementPosition();
    }
    location = ParsingLocation::HeaderName;
    current_header_name = GetLowerCaseTokenValue();
    ScanExpected(Character::Colon);
    ScanOptional(Character::Space);
    location = ParsingLocation::HeaderValue;
}

void
HttpParser::ParseHeaderValue()
{
    SetTokenStartPosition();
    ScanHeaderValue();
    headers[current_header_name] = GetTokenValue();
    ScanOptional(Character::Space);
    ScanExpected(Character::CarriageReturn);
    ScanExpected(Character::NewLine);
    location = ParsingLocation::HeaderName;
}

bool
HttpParser::IsHeaderFieldStart(char ch)
{
    return (ch >= Character::a && ch <= Character::z) ||
           (ch >= Character::A && ch <= Character::Z);
}

bool
HttpParser::IsHeaderFieldPart(char ch)
{
    return (ch >= Character::a && ch <= Character::z) ||
           (ch >= Character::A && ch <= Character::Z) ||
           ch == Character::Dash;
}

bool
HttpParser::IsMethodPart(char ch)
{
    return (ch >= Character::A && ch <= Character::Z);
}

void
HttpParser::ScanRequestTarget()
{
    while (buffer_position < current_buffer_read_length)
    {
        char ch = GetCurrentChar();
        IncrementPosition();

        if (ch == Character::Space) {
            return;
        }
        if (ch == Character::Slash) {
            continue;
        }
        if (IsPchar(ch)) {
            continue;
        }
        if (ch == Character::Question) {
            return;
        }
    }

    throw std::logic_error("Should not reach here.");
}

bool
HttpParser::ScanFieldContent()
{
    Save();
    char ch = GetCurrentChar();
    if (IsVchar(ch) || IsObsText(ch)) {
        IncrementPosition();
    }
    else {
        Revert();
        return false;
    }
    ch = GetCurrentChar();
    if (ch == Space || ch == HorizontalTab) {
        IncrementPosition();
        ch = GetCurrentChar();
        while (buffer_position < current_buffer_read_length && (ch == Space || ch == HorizontalTab)) {
            IncrementPosition();
            ch = GetCurrentChar();
        }
    }
    else {
        return true;
    }
    if (!IsVchar(GetCurrentChar())) {
        Revert();
        return false;
    }
    IncrementPosition();
    return true;
}

void
HttpParser::ScanHeaderValue()
{
    while (buffer_position < current_buffer_read_length) {
        if (ScanFieldContent()) {
            continue;
        }
        if (IsObsText(GetCurrentChar())) {
            IncrementPosition();
            continue;
        }
        return;
    }
}

bool
HttpParser::IsPchar(char ch)
{
    if (IsUnreservedChar(ch)) {
        return true;
    }
    if (IsSubDelimiter(ch)) {
        return true;
    }
    if (ch == Colon) {
        return true;
    }
    if (ch == At) {
        return true;
    }
    if (ch == Percent) {
        IncrementPosition();
        if (!isxdigit(GetCurrentChar())) {
            throw std::logic_error("Expected hex number.");
        }
        IncrementPosition();
        if (!isxdigit(GetCurrentChar())) {
            throw std::logic_error("Expected hex number.");
        }
        return true;
    }
    return false;
}

bool
HttpParser::IsUnreservedChar(char ch)
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

bool
HttpParser::IsSubDelimiter(char ch)
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

bool
HttpParser::IsVchar(char ch)
{
    if (ch >= Exclamation && ch <= Tilde) {
        return true;
    }
    return false;
}

bool
HttpParser::IsObsText(char ch)
{
    return  (ch >= 0x80 && ch <= 0xff);
}

void
HttpParser::IncrementPosition()
{
    buffer_position++;
    if (buffer_position > current_buffer_read_length) {
        throw BufferOverflowException();
    }
}

void
HttpParser::IncrementPosition(std::size_t increment)
{
    buffer_position += increment;
    if (buffer_position > current_buffer_read_length) {
        throw BufferOverflowException();
    }
}

bool
HttpParser::ScanOptional(char ch)
{
    Save();
    if (GetCurrentChar() == ch) {
        IncrementPosition();
        return true;
    }
    Revert();
    return false;
}

bool
HttpParser::NextCharIs(char ch) {
    return ScanOptional(ch);
}

void
HttpParser::ScanExpected(char ch)
{
    if (GetCurrentChar() == ch) {
        IncrementPosition();
        return;
    }
    if (ch == Character::CarriageReturn) {
        throw UnexpectedTokenException(std::string("Expected character '") + "\\r'.");
    }
    if (ch == Character::NewLine) {
        throw UnexpectedTokenException(std::string("Expected character '") + "\\n'.");
    }
    throw UnexpectedTokenException(std::string("Expected character '") + ch + "'");
}


char
HttpParser::PeekNextChar()
{
    char ch;
    Save();
    IncrementPosition();
    ch = GetCurrentChar();
    Revert();
    return ch;
}

void
HttpParser::Save()
{
    SavedTextCursor saved_text_cursor {
        buffer_position,
        start_position,
        end_position,
    };
    saved_text_cursors.push(saved_text_cursor);
}

void
HttpParser::Revert()
{
    const SavedTextCursor& saved_text_cursor = saved_text_cursors.top();
    buffer_position = saved_text_cursor.position;
    start_position = saved_text_cursor.start_position;
    end_position = saved_text_cursor.end_position;
    saved_text_cursors.pop();
}


void
HttpParser::SetTokenStartPosition()
{
    start_position = buffer_position;
}

TokenValue
HttpParser::GetLowerCaseTokenValue()
{
    for (std::size_t i = start_position; i < buffer_position; i++) {
        current_buffer_text[i] = static_cast<char>(tolower(current_buffer_text[i]));
    }
    return {
        &current_buffer_text[start_position],
        buffer_position - start_position
    };
}

TokenValue
HttpParser::GetTokenValue() const
{
    return {
        &current_buffer_text[start_position],
        buffer_position - start_position
    };
}

}
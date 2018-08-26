#ifndef FLASH_HTTP_PARSER_H
#define FLASH_HTTP_PARSER_H

#include <program/http_scanner.h>
#include <unordered_map>
#include <types.h>
#include <uv.h>

using namespace flashpoint::lib;

namespace flashpoint {


struct TokenValue {
    const char *value;
    std::size_t length;
};

struct SavedTextCursor {
    std::size_t position;
    std::size_t start_position;
    std::size_t end_position;
};

enum ParserMode {
    Method,
    AbsolutePath,
    Query,
    HttpVersion,
    HeaderField,
    HeaderValue,
};

enum HttpMethod {
    None,
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head,
    Connect,
    Options,
    Trace,
};

enum class StartLineToken {
    None,
    Crlf,
    Question,

    Protocol,
    AbsolutePath,
    Query,
    HttpVersion1_1,

    EndOfRequestTarget,
};

struct RequestLine {
    HttpMethod method;
    char* path;
    char* query;
};

struct StatusLine {
    unsigned int status_code;
    const char* reason_phrase;
};

struct HttpRequest {
    HttpMethod method;
    char* path;
    char* query;
    std::map<HttpHeader, char*> headers;
    char* body;
};

struct HttpResponse {
    unsigned int status_code;
    std::map<HttpHeader, char*> headers;
    char* body;
    std::size_t body_size;
};

enum class ParsingLocation {
    RequestLine,
    StatusLine,
    HeaderName,
    HeaderValue,
    Body,
    End,
};

class HttpParser final {
public:

    HttpParser();

    void
    ParseRequest(const char *buffer, std::size_t read_length);

    void
    ParseResponse(const char *buffer, std::size_t read_length);

    void
    ParseRequestLine();

    void
    ParseStatusLine();

    void
    ParseDefaultLocations();

    bool
    IsFinished();

    void
    ParseBody();

    void
    AddBuffer(const char *buffer, std::size_t read_length);

    void
    ScanRequestTarget();

    void
    ParseHeaderName();

    void
    ParseHeaderValue();

    TokenValue
    ScanAbsolutePath();

    TokenValue
    ScanQuery();

    TokenValue
    ScanBody();

    StartLineToken
    ScanHttpVersion();

    TokenValue
    ScanReasonPhrase();

    HttpMethod
    ScanMethod();

    TokenValue
    GetLowerCaseTokenValue();

    TokenValue
    GetTokenValue() const;

    unsigned int
    ScanPositiveInteger();

    bool
    ScanOptional(char ch);

    void
    ScanExpected(char ch);

    bool
    NextCharIs(char ch);

private:
    HttpScanner scanner;

    bool has_content_length;

    std::size_t content_length;

    unsigned int status_code;

    TokenValue reason_phrase;

    std::vector<TokenValue> body;

    TokenValue current_header_name;

    std::map<TokenValue, TokenValue> headers;

    ParsingLocation location = ParsingLocation::StatusLine;

    unsigned int length;

    std::size_t buffer_index = 0;

    std::size_t current_buffer_read_length;

    const char *current_buffer_text;

    std::size_t buffer_position = 0;

    std::size_t start_position = 0;

    std::size_t end_position = 0;

    std::size_t body_length = 0;

    TokenValue path;

    TokenValue query;

    ParserMode parser_mode;

    std::stack<SavedTextCursor> saved_text_cursors;

    std::size_t size;

    const std::map<HttpHeader, const char*>
        header_enum_to_string;

    const std::map<const char*, HttpHeader, char_compare>
        header_to_token_enum;

    bool
    IsDigit(char32_t ch);

    bool
    IsPchar(char ch);

    bool
    IsDigit(char ch);

    void
    Save();

    void
    Revert();

    char
    PeekNextChar();

    bool
    ScanFieldContent();

    void
    ScanHeaderValue();

    bool
    IsVchar(char ch);

    bool
    IsObsText(char ch);

    bool
    IsUnreservedChar(char ch);

    bool
    IsSubDelimiter(char ch);

    bool
    IsHeaderFieldStart(char ch);

    bool
    IsHeaderFieldPart(char ch);

    bool
    IsMethodPart(char ch);

    void
    IncrementPosition();

    void
    SetTokenStartPosition();

    char
    GetCurrentChar();

    HttpHeader
    GetHeader(char *ch);
};

}


#endif //FLASH_HTTP_PARSER_H

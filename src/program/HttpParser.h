#ifndef FLASH_HTTP_PARSER_H
#define FLASH_HTTP_PARSER_H

#include <unordered_map>
#include <types.h>
#include <stack>
#include <algorithm>
#include <uv.h>

namespace flashpoint {


struct TokenValue {
    const char *value;
    std::size_t length;
};

struct TokenValueComparer {
    bool operator()(const TokenValue& a, const TokenValue& b) const
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

enum class ParsingLocation {
    StartLine,
    HeaderName,
    HeaderValue,
    Body,
    End,
};

class HttpParser final {
public:

    HttpParser();

    void
    ParseRequest(char *buffer, std::size_t read_length);

    void
    ParseResponse(char *buffer, std::size_t read_length);

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
    AddBuffer(char *buffer, std::size_t read_length);

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

    std::vector<TokenValue> body;

private:

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
    IncrementPosition(std::size_t increment);

    void
    SetTokenStartPosition();

    char
    GetCurrentChar();

    TokenValue*
    GetHeader(const char* name);

    std::size_t
    ToUint(TokenValue* token_value);

    bool has_content_length;

    std::size_t content_length;

    unsigned int status_code;

    TokenValue reason_phrase;

    TokenValue current_header_name;

    std::map<TokenValue, TokenValue, TokenValueComparer> headers;

    ParsingLocation location = ParsingLocation::StartLine;

    unsigned int length;

    std::size_t buffer_index = 0;

    std::size_t current_buffer_read_length;

    char *current_buffer_text;

    std::size_t buffer_position = 0;

    std::size_t start_position = 0;

    std::size_t end_position = 0;

    std::size_t body_length = 0;

    HttpMethod method;

    TokenValue path;

    TokenValue query;

    ParserMode parser_mode;

    std::stack<SavedTextCursor> saved_text_cursors;

    std::size_t size;
};

}


#endif //FLASH_HTTP_PARSER_H

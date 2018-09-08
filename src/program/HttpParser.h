#ifndef FLASH_HTTP_PARSER_H
#define FLASH_HTTP_PARSER_H

#include <unordered_map>
#include <types.h>
#include <stack>
#include <algorithm>
#include <uv.h>
#include <lib/text_span.h>

namespace flashpoint {

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
    ParseRequest(TextSpan *text_span);

    void
    ParseRequestTest(TextSpan *text_span);

    void
    ParseResponse(TextSpan *text_span);

    void
    ParseResponse(std::vector<TextSpan*> &text_spans);

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
    ScanRequestTarget();

    void
    ParseHeaderName();

    void
    ParseHeaderValue();

    TextSpan*
    ScanAbsolutePath();

    TextSpan*
    ScanQuery();

    TextSpan*
    ScanBody();

    StartLineToken
    ScanHttpVersion();

    TextSpan*
    ScanReasonPhrase();

    HttpMethod
    ScanMethod();

    TextSpan*
    GetLowerCaseTokenValue();

    TextSpan*
    GetTokenValue() const;

    unsigned int
    ScanPositiveInteger();

    bool
    ScanOptional(char ch);

    void
    ScanExpected(char ch);

    bool
    NextCharIs(char ch);

    std::map<TextSpan*, TextSpan*, TextSpanComparer> header;

    std::vector<TextSpan*> body;

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

    TextSpan*
    GetHeader(const char* name);

    std::size_t
    ToUint(TextSpan* token_value);

    TextSpan *current_text_span;

    bool has_content_length;

    std::size_t content_length;

    unsigned int status_code;

    TextSpan* reason_phrase;

    TextSpan* current_header_name;

    ParsingLocation location = ParsingLocation::StartLine;

    unsigned int length;

    std::size_t buffer_index = 0;

    std::size_t text_span_position = 0;

    std::size_t start_position = 0;

    std::size_t end_position = 0;

    std::size_t body_length = 0;

    HttpMethod method;

    TextSpan* path;

    TextSpan* query;

    ParserMode parser_mode;

    std::stack<SavedTextCursor> saved_text_cursors;

    std::size_t size;
};

}


#endif //FLASH_HTTP_PARSER_H

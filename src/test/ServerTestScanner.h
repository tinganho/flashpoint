#ifndef FLASHPOINT_SERVERTESTSCANNER_H
#define FLASHPOINT_SERVERTESTSCANNER_H

#include <glibmm/ustring.h>
#include <program/diagnostic.h>
#include <program/HttpParser.h>
#include "ScanningTextCursor.h"
#include <vector>
#include <map>
#include <lib/text_span.h>

using namespace flashpoint::program;
namespace flashpoint::test {

enum class ServerTestToken {
    Unknown,

    Colon,
    Space,
    Newline,
    Identifier,
    IntegerLiteral,

    BackendDirective,
    EndDirective,
    OnGraphQlFieldDirective,
    RequestDirective,
    EndOfDocument,
};

extern const std::map<const TextSpan*, const ServerTestToken, TextSpanComparer> server_test_string_to_token;
extern const std::map<const ServerTestToken, const TextSpan*> server_test_token_to_string;

struct GraphqlResponse {
    TextSpan *field;
    std::map<TextSpan*, TextSpan*, TextSpanComparer> header;
    std::vector<TextSpan*> body;
};

struct Request {
    TextSpan *hostname;
    TextSpan *port;
    std::map<TextSpan*, TextSpan*, TextSpanComparer> header;
    std::vector<TextSpan*> body;
};

struct Backend {
    TextSpan *hostname;
    TextSpan *port;
    std::vector<GraphqlResponse> graphql_responses;
    int pipe_fd[2];
    pid_t child_pid;
};

enum class ServerTestScanningLocation {
    Backend,
    BackendBody,
    Request,
};

class ServerTestScanner : public DiagnosticTrait<ServerTestScanner> {
public:

    ServerTestScanner(const char* source, std::size_t size);

    void
    Scan();

    Location
    GetTokenLocation();

    std::vector<Backend*> backends;

    Request request;

private:

    const char *source_;

    std::size_t size_;

    std::size_t position_ = 0;

    std::size_t line_ = 1;

    std::size_t column_ = 1;

    std::size_t token_start_position_;

    std::size_t token_start_line_;

    std::size_t token_start_column_;

    Backend* current_backend_;

    std::map<TextSpan*, TextSpan*> current_graphql_fields_;

    std::stack<ScanningTextCursor> saved_text_cursors;

    ServerTestScanningLocation location_ = ServerTestScanningLocation::Backend;

    void
    IncrementPosition();

    void
    SaveCurrentLocation();

    void
    RevertToPreviousLocation();

    TextSpan*
    GetTokenValue();

    std::size_t
    GetTokenLength();

    ServerTestToken
    TakeNextToken();

    void
    SetTokenStartPosition();

    char32_t
    GetCurrentChar();

    void
    SkipWhiteSpace();

    bool
    IsWhitespace(char32_t ch);

    bool
    ScanExpected(ServerTestToken token);

    bool
    ScanExpected(ServerTestToken token, bool add_diagnostic);

    bool
    IsIdentifierStart(const char32_t &ch) const;

    bool
    IsIdentifierPart(const char32_t &ch) const;

    ServerTestToken
    GetTokenFromValue(const TextSpan *value) const;

    bool
    ValidateHostname(const TextSpan *hostname);

    bool
    ValidatePort(const TextSpan *port);

    void
    SkipRestOfLine();

    void
    ScanContent();

    ServerTestToken
    ScanInteger();

    void
    ScanDigitList();

    bool
    IsDigit(char32_t ch);
};

}

#endif //FLASHPOINT_SERVERTESTSCANNER_H

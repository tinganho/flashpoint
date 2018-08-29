#ifndef FLASHPOINT_SERVERTESTSCANNER_H
#define FLASHPOINT_SERVERTESTSCANNER_H

#include <glibmm/ustring.h>
#include <program/diagnostic.h>
#include <vector>
#include <map>

using namespace flashpoint::program;
namespace flashpoint::test {

enum class ServerTestToken {
    Unknown,

    Colon,
    Space,
    Newline,
    Identifier,

    BackendDirective,
    EndDirective,
    OnGraphQlFieldDirective,
    RequestDirective,
    EndOfDocument,
};

extern const std::map<const Glib::ustring, const ServerTestToken> server_test_string_to_token;
extern const std::map<const ServerTestToken, const Glib::ustring> server_test_token_to_string;

struct Backend {
    const char* hostname;
    const char* port;
    bool https;
};

class ServerTestScanner : public DiagnosticTrait<ServerTestScanner> {
public:
    ServerTestScanner(const Glib::ustring& source);

    void
    Scan();

    std::vector<Backend> backends;

private:
    Glib::ustring source;
    std::size_t position = 0;
    std::size_t line = 1;
    std::size_t size;
    std::size_t column = 1;
    std::size_t token_start_position;
    std::size_t token_start_line;
    std::size_t token_start_column;
    Glib::ustring hostname;
    Glib::ustring port;
    Glib::ustring current_field;
    Glib::ustring current_content;

    void
    IncrementPosition();

    Glib::ustring
    GetTokenValue();

    std::size_t
    GetTokenLength();

    ServerTestToken
    TakeNextToken();

    void
    SetTokenStartPosition();

    char32_t
    GetCurrentChar();

    bool
    ScanExpected(ServerTestToken token);

    bool
    ScanExpected(ServerTestToken token, bool add_diagnostic);

    bool
    IsIdentifierStart(const char32_t &ch) const;

    bool
    IsIdentifierPart(const char32_t &ch) const;

    ServerTestToken
    GetTokenFromValue(const Glib::ustring& value);

    bool
    ValidateHostname(const Glib::ustring &hostname);

    bool
    ValidatePort(const Glib::ustring &port);

    void
    SkipRestOfLine();

    void
    ScanContent();
};

}

#endif //FLASHPOINT_SERVERTESTSCANNER_H

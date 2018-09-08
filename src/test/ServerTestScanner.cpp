#include "ServerTestScanner.h"
#include "test_case_diagnostics.h"
#include "GraphQlTestCaseScanner.h"
#include "TestExceptions.h"
#include <lib/character.h>
#include <boost/algorithm/string_regex.hpp>
#include <program/HttpParser.h>

#define TOKEN_VALUE(x) new TextSpan { x, strlen(x) }

using namespace flashpoint::lib;

namespace flashpoint::test {

const std::map<const TextSpan*, const ServerTestToken, TextSpanComparer> server_test_string_to_token = {
    { TOKEN_VALUE(":"), ServerTestToken::Colon },
    { TOKEN_VALUE("BACKEND"), ServerTestToken::BackendDirective },
    { TOKEN_VALUE("END"), ServerTestToken::EndDirective },
    { TOKEN_VALUE("ON_GRAPHQL_FIELD"), ServerTestToken::OnGraphQlFieldDirective },
    { TOKEN_VALUE("REQUEST"), ServerTestToken::RequestDirective },
};

const std::map<const ServerTestToken, const TextSpan*> server_test_token_to_string = {
    { ServerTestToken::Colon, TOKEN_VALUE(":") },
    { ServerTestToken::BackendDirective, TOKEN_VALUE("BACKEND") },
    { ServerTestToken::EndDirective, TOKEN_VALUE("END") },
    { ServerTestToken::OnGraphQlFieldDirective, TOKEN_VALUE("ON_GRAPHQL_FIELD") },
    { ServerTestToken::RequestDirective, TOKEN_VALUE("REQUEST") },
};

ServerTestScanner::ServerTestScanner(const char *source, std::size_t size):
    source_(source),
    size_(size)
{ }

void
ServerTestScanner::Scan() {
    outer: while(true) {
        ServerTestToken token = TakeNextToken();
        if (token == ServerTestToken::EndOfDocument) {
            return;
        }
        switch (location_) {
            case ServerTestScanningLocation::Backend:
                switch (token) {
                    case ServerTestToken::BackendDirective: {
                        ScanExpected(ServerTestToken::Space);
                        if (!ScanExpected(ServerTestToken::Identifier, false)) {
                            AddDiagnostic(D::Expected_hostname);
                            SkipRestOfLine();
                            location_ = ServerTestScanningLocation::BackendBody;
                            break;
                        }
                        auto hostname = GetTokenValue();
                        ValidateHostname(hostname);
                        ScanExpected(ServerTestToken::Space);
                        if (!ScanExpected(ServerTestToken::IntegerLiteral, false)) {
                            AddDiagnostic(D::Expected_port);
                            SkipRestOfLine();
                            location_ = ServerTestScanningLocation::BackendBody;
                            break;
                        }
                        auto port = GetTokenValue();
                        ValidatePort(port);
                        ScanExpected(ServerTestToken::Colon);
                        current_backend_ = new Backend {
                            hostname,
                            port,
                            {},
                        };
                        backends.push_back(current_backend_);
                        location_ = ServerTestScanningLocation::BackendBody;
                        break;
                    }
                    default:
                        AddDiagnostic(D::Expected_0_directive, "BACKEND");
                        return;
                }
                return Scan();
            case ServerTestScanningLocation::BackendBody:
                switch (token) {
                    case ServerTestToken::EndDirective:
                        location_ = ServerTestScanningLocation::Request;
                        return Scan();
                    case ServerTestToken::Newline:
                        return Scan();
                    case ServerTestToken::OnGraphQlFieldDirective: {
                        ScanExpected(ServerTestToken::Space);
                        if (!ScanExpected(ServerTestToken::Identifier, false)) {
                            AddDiagnostic(D::Expected_hostname);
                            SkipRestOfLine();
                            goto outer;
                        }
                        auto graphql_field = GetTokenValue();
                        ScanExpected(ServerTestToken::Colon);
                        ScanExpected(ServerTestToken::Newline);
                        ScanContent();
                        auto http_request_source = GetTokenValue();
                        HttpParser http_parser;
                        http_parser.ParseRequestTest(http_request_source);
                        SkipWhiteSpace();
                        ScanExpected(ServerTestToken::EndDirective);
                        current_backend_->graphql_responses.push_back(GraphqlResponse {
                            graphql_field,
                            http_parser.header,
                            http_parser.body
                        });
                        return Scan();
                    }
                    default:
                        AddDiagnostic(D::Expected_0_directive, "ON_GRAPHQL_FIELD");
                        return;
                }
                throw std::logic_error("Should not reach here.");
            case ServerTestScanningLocation::Request:;
                switch (token) {
                    case ServerTestToken::RequestDirective: {
                        ScanExpected(ServerTestToken::Space);
                        if (!ScanExpected(ServerTestToken::Identifier, false)) {
                            AddDiagnostic(D::Expected_hostname);
                            SkipRestOfLine();
                            location_ = ServerTestScanningLocation::BackendBody;
                            break;
                        }
                        auto hostname = GetTokenValue();
                        ValidateHostname(hostname);
                        ScanExpected(ServerTestToken::Space);
                        if (!ScanExpected(ServerTestToken::IntegerLiteral, false)) {
                            AddDiagnostic(D::Expected_port);
                            SkipRestOfLine();
                            location_ = ServerTestScanningLocation::BackendBody;
                            break;
                        }
                        auto port = GetTokenValue();
                        ValidatePort(port);
                        ScanExpected(ServerTestToken::Colon);
                        ScanExpected(ServerTestToken::Newline);
                        ScanContent();
                        auto http_request_source = GetTokenValue();
                        HttpParser http_parser;
                        http_parser.ParseRequestTest(http_request_source);
                        request = {
                            hostname,
                            port,
                            http_parser.header,
                            http_parser.body,
                        };
                        return;
                    }
                    case ServerTestToken::Newline:
                        return Scan();
                }
                break;
            default:;
        }
    }


}

Location
ServerTestScanner::GetTokenLocation()
{
    return Location {
        token_start_line_,
        token_start_column_,
        GetTokenLength(),
        position_ >= size_,
    };
}

bool
ServerTestScanner::ValidateHostname(const TextSpan *hostname)
{
    if (hostname->length > 253) {
        AddDiagnostic(D::Hostname_cannot_exceed_255_characters);
        return false;
    }

    boost::regex rgx("(?!-)[A-Z\\d-]{1,63}(?<!-)$");
    boost::smatch match;
    const std::string shostname = ToCharArray(hostname);
    if (!boost::regex_match(shostname.begin(), shostname.end(), match, rgx)) {
        AddDiagnostic(D::Invalid_hostname);
        return false;
    }
    return true;
}

bool
ServerTestScanner::ValidatePort(const TextSpan *port)
{

}

ServerTestToken
ServerTestScanner::TakeNextToken()
{
    SetTokenStartPosition();
    while (position_ < size_) {
        char32_t ch = GetCurrentChar();
        IncrementPosition();
        switch (ch) {
            case Character::CarriageReturn:
                if (GetCurrentChar() == Character::Newline) {
                    IncrementPosition();
                    return ServerTestToken::Newline;
                }
                return ServerTestToken::Unknown;
            case Character::Space:
                return ServerTestToken::Space;
            case Character::Colon:
                return ServerTestToken::Colon;
            case Character::_0:
            case Character::_1:
            case Character::_2:
            case Character::_3:
            case Character::_4:
            case Character::_5:
            case Character::_6:
            case Character::_7:
            case Character::_8:
            case Character::_9:
                return ScanInteger();
            default:
                if (IsIdentifierStart(ch)) {
                    while (position_ < size_ && IsIdentifierPart(GetCurrentChar())) {
                        IncrementPosition();
                    }
                    return GetTokenFromValue(GetTokenValue());
                }
                return ServerTestToken::Unknown;
        }
    }
    return ServerTestToken::EndOfDocument;
}

ServerTestToken
ServerTestScanner::ScanInteger()
{
    ScanDigitList();
    return ServerTestToken::IntegerLiteral;
}

void
ServerTestScanner::ScanDigitList()
{
    while (IsDigit(GetCurrentChar())) {
        IncrementPosition();
    }
}

bool
ServerTestScanner::IsDigit(char32_t ch)
{
    return ch >= Character::_0 && ch <= Character::_9;
}

char32_t
ServerTestScanner::GetCurrentChar()
{
    return source_[position_];
}

TextSpan*
ServerTestScanner::GetTokenValue()
{
    return new TextSpan {
        &source_[token_start_position_],
        GetTokenLength(),
    };
}

void
ServerTestScanner::IncrementPosition()
{
    column_++;
    position_++;
}

std::size_t
ServerTestScanner::GetTokenLength()
{
    return position_ - token_start_position_;
}

void
ServerTestScanner::SetTokenStartPosition()
{
    token_start_position_ = position_;
}
void
ServerTestScanner::SkipWhiteSpace()
{
    while (position_ < size_ && IsWhitespace(GetCurrentChar())) {
        IncrementPosition();
    }
}

bool
ServerTestScanner::IsWhitespace(char32_t ch)
{
    return ch == Character::CarriageReturn || ch == Character::Newline || ch == Character::Space;
}

bool
ServerTestScanner::ScanExpected(ServerTestToken token)
{
    return ScanExpected(token, true);
}

bool
ServerTestScanner::ScanExpected(ServerTestToken token, bool add_diagnostic)
{
    ServerTestToken result = TakeNextToken();
    if (result != token) {
        if (add_diagnostic) {
            AddDiagnostic(
                D::Expected_0_but_got_1,
                ToCharArray(server_test_token_to_string.at(token)),
                ToCharArray(GetTokenValue())
            );
        }
        return false;
    }
    return true;
}

ServerTestToken
ServerTestScanner::GetTokenFromValue(const TextSpan *value) const
{
    auto result = server_test_string_to_token.find(value);
    if (result != server_test_string_to_token.end()) {
        return result->second;
    }
    return ServerTestToken::Identifier;
}

bool
ServerTestScanner::IsIdentifierStart(const char32_t &ch) const
{
    return (ch >= Character::A && ch <= Character::Z) ||
        (ch >= Character::a && ch <= Character::z) ||
        ch == Character::_;
}

bool
ServerTestScanner::IsIdentifierPart(const char32_t &ch) const
{
    return (ch >= Character::a && ch <= Character::z) ||
        (ch >= Character::A && ch <= Character::Z) ||
        (ch >= Character::_0 && ch <= Character::_9) ||
        ch == Character::_;
}

void
ServerTestScanner::SkipRestOfLine()
{
    char32_t ch;
    while (position_ < size_) {
        ch = GetCurrentChar();
        IncrementPosition();
        switch (ch) {
            case Character::Newline:
                return;
            case Character::CarriageReturn:
                return;
        }
    }
}

void
ServerTestScanner::ScanContent()
{
    SetTokenStartPosition();
    while (true) {
        SaveCurrentLocation();
        auto ch = GetCurrentChar();
        if (ch == Character::CarriageReturn) {
            IncrementPosition();
            if (GetCurrentChar() != Character::Newline) {
                AddDiagnostic(D::Expected_0_directive, "newline");
                return;
            }
            IncrementPosition();
            ch = GetCurrentChar();
        }
        if (ch == Character::E) {
            Glib::ustring candidate_string = "E";
            IncrementPosition();
            ch = GetCurrentChar();
            while (position_ < size_ && IsIdentifierPart(ch)) {
                IncrementPosition();
                candidate_string += ch;
                ch = GetCurrentChar();
            }
            if (candidate_string == "END") {
                RevertToPreviousLocation();
                return;
            }
        }
        IncrementPosition();
    }
}



void
ServerTestScanner::SaveCurrentLocation()
{
    saved_text_cursors.emplace(
        position_,
        line_,
        column_,
        token_start_position_,
        token_start_line_,
        token_start_column_
    );
}

void
ServerTestScanner::RevertToPreviousLocation()
{
    auto saved_text_cursor = saved_text_cursors.top();
    position_ = saved_text_cursor.position;
    line_ = saved_text_cursor.line;
    column_ = saved_text_cursor.column;
    token_start_position_ = saved_text_cursor.token_start_position;
    token_start_line_ = saved_text_cursor.token_start_line;
    token_start_column_ = saved_text_cursor.token_start_column;
    saved_text_cursors.pop();
}

}
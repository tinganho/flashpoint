#include "ServerTestScanner.h"
#include "test_case_diagnostics.h"
#include "GraphQlTestCaseScanner.h"
#include "TestExceptions.h"
#include <lib/character.h>
#include <boost/algorithm/string_regex.hpp>


using namespace flashpoint::lib;

namespace flashpoint::test {

const std::map<const Glib::ustring, const ServerTestToken> server_test_string_to_token = {
    { ":", ServerTestToken::Colon },
    { "BACKEND", ServerTestToken::BackendDirective },
    { "END", ServerTestToken::EndDirective },
    { "ON_GRAPHQL_FIELD", ServerTestToken::OnGraphQlFieldDirective },
    { "REQUEST", ServerTestToken::RequestDirective },
};

ServerTestScanner::ServerTestScanner(const Glib::ustring &source):
    source(source),
    size(source.size())
{ }

void
ServerTestScanner::Scan() {
    outer: while(true) {
        ServerTestToken token = TakeNextToken();
        switch (token) {
            case ServerTestToken::BackendDirective:
                ScanExpected(ServerTestToken::Space);
                if (!ScanExpected(ServerTestToken::Identifier, false)) {
                    AddDiagnostic(D::Expected_hostname);
                    SkipRestOfLine();
                    goto outer;
                }
                hostname = GetTokenValue();
                ValidateHostname(hostname);
                ScanExpected(ServerTestToken::Space);
                if (!ScanExpected(ServerTestToken::Identifier, false)) {
                    AddDiagnostic(D::Expected_port);
                    SkipRestOfLine();
                    goto outer;
                }
                port = GetTokenValue();
                ValidatePort(port);
                ScanExpected(ServerTestToken::Colon);
                break;
            case ServerTestToken::OnGraphQlFieldDirective:
                ScanExpected(ServerTestToken::Space);
                if (!ScanExpected(ServerTestToken::Identifier, false)) {
                    AddDiagnostic(D::Expected_hostname);
                    SkipRestOfLine();
                    goto outer;
                }
                current_field = GetTokenValue();
                ScanExpected(ServerTestToken::Colon);
                ScanExpected(ServerTestToken::Newline);
                ScanContent();
                break;
            case ServerTestToken::RequestDirective:
                ScanExpected(ServerTestToken::Colon);
                ScanExpected(ServerTestToken::Newline);
                ScanContent();
                break;
            case ServerTestToken::EndOfDocument:
                goto outer;
            default:
                throw std::logic_error("Should not reach here.");
        }
    }


}

bool
ServerTestScanner::ValidateHostname(const Glib::ustring &hostname)
{
    if (hostname.length() > 253) {
        AddDiagnostic(D::Hostname_cannot_exceed_255_characters);
        return false;
    }

    boost::regex rgx("(?!-)[A-Z\\d-]{1,63}(?<!-)$");
    boost::smatch match;
    const std::string shostname = std::string(hostname.c_str());
    if (!boost::regex_match(shostname.begin(), shostname.end(), match, rgx)) {
        AddDiagnostic(D::Invalid_hostname);
        return false;
    }
    return true;
}

bool
ServerTestScanner::ValidatePort(const Glib::ustring &port)
{

}

ServerTestToken
ServerTestScanner::TakeNextToken()
{
    SetTokenStartPosition();
    while (position < size) {
        char32_t ch = GetCurrentChar();
        IncrementPosition();
        switch (ch) {
            case Character::Space:
                return ServerTestToken::Space;
            case Character::Colon:
                return ServerTestToken::Colon;
            default:
                if (IsIdentifierStart(ch)) {
                    std::size_t size = 1;
                    while (position < size && IsIdentifierPart(GetCurrentChar())) {
                        IncrementPosition();
                        size++;
                    }
                    return GetTokenFromValue(GetTokenValue());
                }
                return ServerTestToken::Unknown;
        }
    }
    return ServerTestToken::EndOfDocument;
}

char32_t
ServerTestScanner::GetCurrentChar()
{
    return source[position];
}

Glib::ustring
ServerTestScanner::GetTokenValue()
{
    return source.substr(token_start_position, GetTokenLength());
}

void
ServerTestScanner::IncrementPosition()
{
    column++;
    position++;
}

std::size_t
ServerTestScanner::GetTokenLength()
{
    return position - token_start_position;
}

void
ServerTestScanner::SetTokenStartPosition()
{
    token_start_position = position;
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
            AddDiagnostic(D::Expected_0_but_got_1, server_test_token_to_string.at(token), GetTokenValue());
        }
        return false;
    }
    return true;
}

ServerTestToken
ServerTestScanner::GetTokenFromValue(const Glib::ustring& value)
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
    while (position < size) {
        ch = GetCurrentChar();
        IncrementPosition();
        switch (ch) {
            case Character::NewLine:
                return;
            case Character::CarriageReturn:
                return;
        }
    }
}

void
ServerTestScanner::ScanContent()
{
    auto start_of_content = position;
    while (true) {
        auto token = TakeNextToken();
        switch (token) {
            case ServerTestToken::EndDirective:
                current_content = source.substr(start_of_content, position - start_of_content);
            case ServerTestToken::EndOfDocument:
                return;
            default:;
        }
    }
}

}
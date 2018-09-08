#ifndef FLASHPOINT_TEST_CASE_SCANNER_H
#define FLASHPOINT_TEST_CASE_SCANNER_H

#include <glibmm/ustring.h>
#include <program/diagnostic.h>
#include "ScanningTextCursor.h"
#include <map>

using namespace flashpoint::program;

namespace flashpoint::test {

enum class TestCaseToken {
    Unknown,

    TestDirective,
    EndDirective,
    ForEachDirective,

    CasesKeyword,
    EndKeyword,
    OpenBracket,
    OpenParen,
    CloseBracket,
    OpenBrace,
    CloseBrace,
    CloseParen,
    Colon,
    Comma,
    StringLiteral,
    Hash,
    At,
    Identifier,

    WhiteSpace,

    EndOfDocument,
};

const std::map<Glib::ustring, const TestCaseToken> test_case_string_to_token = {
    { "@end", TestCaseToken::EndDirective },
    { "@foreach", TestCaseToken::ForEachDirective },
    { "@test", TestCaseToken::TestDirective },
    { ":", TestCaseToken::Colon },
};

const std::map<const TestCaseToken, Glib::ustring> test_case_token_to_string = {
    { TestCaseToken::TestDirective, "@test" },
    { TestCaseToken::EndKeyword, "@end" },
    { TestCaseToken::Colon, ":" },
};

struct TestCaseArguments {
    std::map<std::string, std::string> file_arguments;
    std::map<std::string, std::string> source_code_arguments;
};

class GraphQlTestCaseScanner : public DiagnosticTrait<GraphQlTestCaseScanner> {
public:
    GraphQlTestCaseScanner(const Glib::ustring& source);

    std::vector<TestCaseArguments*>
    Scan();

    Location
    GetTokenLocation();

    std::string
    GetSourceCode(std::map<std::string, std::string> source_code_arguments);

    std::string
    GetFilename(std::string filename_template, std::map<std::string, std::string> file_arguments);

private:
    Glib::ustring source;
    Glib::ustring string_literal;
    Glib::ustring source_code_template;
    std::size_t position = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t token_start_position;
    std::size_t token_start_line;
    std::size_t token_start_column;
    std::size_t size;
    std::size_t newline_position;
    std::vector<TestCaseArguments*> scanned_arguments;
    std::stack<ScanningTextCursor> saved_text_cursors;
    bool in_definition_location = false;

    TestCaseToken
    TakeNextToken(bool skip_white_space);

    TestCaseToken
    TakeNextToken();

    void
    IncrementPosition();

    void
    SetTokenStartPosition();

    Glib::ustring
    GetTokenValue();

    std::size_t
    GetTokenLength();

    Glib::ustring
    GetStringLiteral();

    char32_t
    GetCurrentCharacter();

    bool
    IsIdentifierStart(const char32_t &ch) const;

    bool
    IsIdenitiferPart(const char32_t &ch) const;

    TestCaseToken
    ScanStringLiteral();

    bool
    ScanExpectedToken(TestCaseToken token);

    TestCaseToken
    TryScanToken(const TestCaseToken &token);

    bool
    ScanOptionalToken(const TestCaseToken &token);

    void
    SaveCurrentLocation();

    void
    RevertToPreviousLocation();

    std::vector<std::string>
    ScanParametersAfterOpenParen();

    TestCaseToken
    GetTokenFromValue(Glib::ustring value, std::size_t size);

    std::vector<TestCaseArguments*>
    parse_test_cases();

    TestCaseArguments*
    ParseTestCaseArguments();

    std::map<std::string, std::string>*
    ParseReplacementArguments();

    void
    ScanEscapeSequence();

    std::map<std::string, std::string>*
    ParseContentArguments();

    void
    parse_source_code_arguments(std::vector<TestCaseArguments*>& test_cases);

    void
    ReplaceVariableTestCase(
        const std::vector<std::vector<std::map<std::string, std::string> *>> &foreach_statements,
        const std::vector<TestCaseArguments *> &noexpanded_test_cases,
        std::vector<TestCaseArguments *> &expanded_test_cases,
        std::map<std::string, std::string> &replacements,
        std::size_t index
    );
};

}


#endif // FLASHPOINT_TEST_CASE_SCANNER_H
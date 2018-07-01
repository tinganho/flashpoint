#ifndef FLASHPOINT_TEST_CASE_SCANNER_H
#define FLASHPOINT_TEST_CASE_SCANNER_H

#include <glibmm/ustring.h>
#include <program/diagnostic.h>
#include <map>

using namespace flashpoint::program;

namespace flashpoint::test {

    enum class TestCaseToken {
        Unknown,

        CasesDirective,
        EndDirective,

        CasesKeyword,
        EndKeyword,
        OpenBracket,
        CloseBracket,
        OpenBrace,
        CloseBrace,
        Colon,
        Comma,
        StringLiteral,
        Hash,
        At,
        Identifier,

        WhiteSpace,

        EndOfDocument,
    };

    const std::map<Glib::ustring, const TestCaseToken> testCaseStringToToken = {
        { "@cases", TestCaseToken::CasesDirective },
        { "@end", TestCaseToken::EndDirective },
        { ":", TestCaseToken::Colon },
    };

    const std::map<const TestCaseToken, Glib::ustring> testCaseTokenToString = {
        { TestCaseToken::CasesKeyword, "cases" },
        { TestCaseToken::EndKeyword, "end" },
        { TestCaseToken::Colon, ":" },
        { TestCaseToken::At, "@" },
    };

    struct TestCaseArguments {
        std::map<std::string, std::string> file_arguments;
        std::map<std::string, std::string> source_code_arguments;
    };

    class TestCaseScanner : public DiagnosticTrait<TestCaseScanner> {
    public:
        TestCaseScanner(const Glib::ustring& source);

        std::vector<TestCaseArguments*>
        scan();

        Location
        get_token_location();

        std::string
        get_source_code(std::map<std::string, std::string> source_code_arguments);

        std::string
        get_filename(std::string filename_template, std::map<std::string, std::string> file_arguments);

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
        bool in_definition_location = false;

        TestCaseToken
        take_next_token(bool skip_white_space);

        TestCaseToken
        take_next_token();

        void
        increment_position();

        void
        set_token_start_position();

        Glib::ustring
        get_token_value();

        std::size_t
        get_token_length();

        Glib::ustring
        get_string_literal();

        char32_t
        current_char();

        bool
        is_identifier_start(const char32_t& ch) const;

        bool
        is_identifier_part(const char32_t& ch) const;

        TestCaseToken
        scan_string_literal();

        bool
        scan_expected(TestCaseToken token);

        TestCaseToken
        get_token_from_value(Glib::ustring value, std::size_t size);

        std::vector<TestCaseArguments*>
        parse_test_cases();

        TestCaseArguments*
        parse_test_case_arguments();

        std::map<std::string, std::string>*
        parse_file_arguments();

        std::map<std::string, std::string>*
        parse_content_arguments();

        void
        parse_source_code_arguments(std::vector<TestCaseArguments*>& test_cases);
    };
}


#endif // FLASHPOINT_TEST_CASE_SCANNER_H
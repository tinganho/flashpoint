#include "test_case_scanner.h"
#include <lib/character.h>
#include "test_case_diagnostics.h"

using namespace flashpoint::lib;

namespace flashpoint::test {
    TestCaseScanner::TestCaseScanner(const Glib::ustring& source):
        source(source),
        size(source.size()),
        source_code_template(source)
    { }

    std::vector<TestCaseArguments*>
    TestCaseScanner::scan()
    {
        std::vector<TestCaseArguments*> test_cases = {};
        while(true) {
            TestCaseToken token = take_next_token();
            switch (token) {
                case TestCaseToken::EndOfDocument:
                    goto outer;
                case TestCaseToken::CasesDirective:
                    if (!scan_expected(TestCaseToken::Colon)) {
                        goto outer;
                    }
                    while (true) {
                        token = take_next_token();
                        if (token == TestCaseToken::EndDirective) {
                            break;
                        }
                        if (token != TestCaseToken::OpenBracket) {
                            add_diagnostic(D::Expected_0_but_got_1, "[", get_token_value());
                        }
                        auto t = parse_test_case_arguments();
                        if (t == nullptr) {
                            continue;
                        }
                        test_cases.push_back(t);
                    }
                    goto outer;
                default:;
            }
        }
        outer:;
        return test_cases;
    }

    std::string
    TestCaseScanner::get_filename(std::string filename_template, std::map<std::string, std::string> file_arguments)
    {
        std::string filename = filename_template;
        for (const auto& it : file_arguments) {
            filename = boost::regex_replace(filename, boost::regex("{" + it.first + "}"), it.second);
        }
        return filename;
    }

    std::string
    TestCaseScanner::get_source_code(std::map<std::string, std::string> source_code_arguments)
    {
        std::string source_code = source_code_template;
        for (const auto& it : source_code_arguments) {
            source_code = boost::regex_replace(source_code, boost::regex("{{" + it.first + "}}"), it.second);
        }
        return source_code;
    }

    TestCaseArguments*
    TestCaseScanner::parse_test_case_arguments()
    {
        auto file_arguments = parse_file_arguments();
        if (file_arguments == nullptr) {
            return nullptr;
        }
        if (!scan_expected(TestCaseToken::Colon)) {
            return nullptr;
        }
        auto content_arguments = parse_content_arguments();
        if (content_arguments == nullptr) {
            return nullptr;
        }
        return new TestCaseArguments {
            *file_arguments,
            *content_arguments,
        };
    }

    std::map<std::string, std::string>*
    TestCaseScanner::parse_file_arguments()
    {
        auto file_arguments = new std::map<std::string, std::string>();
        bool can_parse_comma = false;
        while (true) {
            TestCaseToken token = take_next_token();
            if (token == TestCaseToken::EndOfDocument) {
                break;
            }
            if (token == TestCaseToken::CloseBracket) {
                break;
            }
            if (token == TestCaseToken::Comma) {
                if (!can_parse_comma) {
                    add_diagnostic(D::Expected_identifier);
                    continue;
                }
                can_parse_comma = false;
            }
            if (token != TestCaseToken::Identifier) {
                add_diagnostic(D::Expected_identifier);
                return file_arguments;
            }
            auto key = get_token_value();
            if (!scan_expected(TestCaseToken::Colon)) {
                return file_arguments;
            }
            if (!scan_expected(TestCaseToken::StringLiteral)) {
                return file_arguments;
            }
            file_arguments->emplace(key, get_string_literal());
            can_parse_comma = true;
        }

        return file_arguments;
    }

    std::map<std::string, std::string>*
    TestCaseScanner::parse_content_arguments()
    {
        if (!scan_expected(TestCaseToken::OpenBrace)) {
            return nullptr;
        }
        auto content_arguments = new std::map<std::string, std::string>();
        while (true) {
            TestCaseToken token = take_next_token();
            if (token == TestCaseToken::EndOfDocument) {
                break;
            }
            if (token == TestCaseToken::CloseBrace) {
                break;
            }
            if (token == TestCaseToken::Comma) {
                continue;
            }
            if (token != TestCaseToken::Identifier) {
                add_diagnostic(D::Expected_identifier);
                return nullptr;
            }
            auto key = get_token_value();
            if (!scan_expected(TestCaseToken::Colon)) {
                return nullptr;
            }
            if (!scan_expected(TestCaseToken::StringLiteral)) {
                return nullptr;
            }
            content_arguments->emplace(key, get_string_literal());
        }
        return content_arguments;
    }

    TestCaseToken
    TestCaseScanner::take_next_token()
    {
        return take_next_token(true);
    }

    TestCaseToken
    TestCaseScanner::take_next_token(bool skip_white_space)
    {
        bool last_one_was_at_char = false;
        set_token_start_position();
        while (position < size) {
            char32_t ch = current_char();
            increment_position();

            switch (ch) {
                case NewLine:
                    newline_position = position - 1;
                    if (!skip_white_space) {
                        return TestCaseToken::WhiteSpace;
                    }
                    column = 1;
                    token_start_column = 1;
                    token_start_line++;
                    token_start_position++;
                    continue;
                case CarriageReturn:
                    newline_position = position - 1;
                    if (source[position + 1] == NewLine) {
                        increment_position();
                    }
                    if (!skip_white_space) {
                        return TestCaseToken::WhiteSpace;
                    }
                    column = 1;
                    token_start_line++;
                    token_start_column = 1;
                    token_start_position++;
                    continue;
                case ByteOrderMark:
                case Space:
                case Comma:
                    if (!skip_white_space) {
                        return TestCaseToken::WhiteSpace;
                    }
                    token_start_position++;
                    token_start_column++;
                    continue;
                case Character::At:
                    token_start_position = position - 1;
                    last_one_was_at_char = true;
                    continue;
                default:;
            }

            if (!in_definition_location && !last_one_was_at_char) {
                continue;
            }

            switch (ch) {
                case Character::Hash:
                    token_start_position++;
                    token_start_column++;
                    continue;
                case Character::DoubleQuote:
                    return scan_string_literal();
                case Character::Colon:
                    return TestCaseToken::Colon;
                case Character::OpenBracket:
                    return TestCaseToken::OpenBracket;
                case Character::CloseBracket:
                    return TestCaseToken::CloseBracket;
                case Character::OpenBrace:
                    return TestCaseToken::OpenBrace;
                case Character::CloseBrace:
                    return TestCaseToken::CloseBrace;


                default:
                    if (is_identifier_start(ch)) {
                        std::size_t name_size = 1;
                        while (position < size && is_identifier_part(current_char())) {
                            increment_position();
                            name_size++;
                        }
                        auto token = get_token_from_value(get_token_value(), name_size);
                        if (last_one_was_at_char) {
                            switch (token) {
                                case TestCaseToken::CasesDirective:
                                    source_code_template = source.substr(0, newline_position);
                                    in_definition_location = true;
                                    return TestCaseToken::CasesDirective;
                                case TestCaseToken::EndDirective:
                                    return TestCaseToken::EndDirective;
                                default:
                                    set_token_start_position();
                                    continue;
                            }
                        }
                        return TestCaseToken::Identifier;
                    }
                    return TestCaseToken::Unknown;
            }
        }
        return TestCaseToken::EndOfDocument;
    }

    bool
    TestCaseScanner::scan_expected(TestCaseToken token)
    {
        TestCaseToken result = take_next_token();
        if (result != token) {
            add_diagnostic(D::Expected_0_but_got_1, testCaseTokenToString.at(token), testCaseTokenToString.at(result));
            return false;
        }
        return true;
    }

    Glib::ustring
    TestCaseScanner::get_token_value()
    {
        return source.substr(token_start_position, get_token_length());
    }

    Glib::ustring
    TestCaseScanner::get_string_literal()
    {
        return string_literal;
    }

    std::size_t
    TestCaseScanner::get_token_length()
    {
        return position - token_start_position;
    }

    void
    TestCaseScanner::set_token_start_position()
    {
        token_start_position = position;
    }

    TestCaseToken
    TestCaseScanner::scan_string_literal()
    {
        auto start = position;
        string_literal = "";
        while (true) {
            if (position > size) {
                break;
            }
            char32_t ch = current_char();
            if (ch == Character::DoubleQuote) {
                string_literal += source.substr(start, position - start);
                increment_position();
                break;
            }
            increment_position();
        }
        return TestCaseToken::StringLiteral;
    }

    TestCaseToken
    TestCaseScanner::get_token_from_value(Glib::ustring value, std::size_t size)
    {
        auto result = testCaseStringToToken.find(value);
        if (result != testCaseStringToToken.end()) {
            return result->second;
        }
        return TestCaseToken::Identifier;
    }

    Location
    TestCaseScanner::get_token_location()
    {
        return Location {
            token_start_line,
            token_start_column,
            get_token_length(),
            position >= size,
        };
    }

    bool
    TestCaseScanner::is_identifier_start(const char32_t &ch) const
    {
        return (ch >= A && ch <= Z) ||
               (ch >= a && ch <= z) ||
               ch == _;
    }

    bool
    TestCaseScanner::is_identifier_part(const char32_t &ch) const
    {
        return (ch >= a && ch <= z) ||
               (ch >= A && ch <= Z) ||
               (ch >= _0 && ch <= _9) ||
               ch == _;
    }

    char32_t
    TestCaseScanner::current_char()
    {
        return source[position];
    }

    void
    TestCaseScanner::increment_position()
    {
        position++;
    }
}
#include "test_case_scanner.h"
#include <lib/character.h>
#include "test_case_diagnostics.h"
#include <lib/utils.h>

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
        std::vector<TestCaseArguments*> noexpanded_test_cases = {};
        std::vector<std::string> for_each_arguments = {};
        std::vector<std::vector<std::map<std::string, std::string>*>> foreach_statements = {};
        while(true) {
            TestCaseToken token = take_next_token();
            switch (token) {
                case TestCaseToken::EndOfDocument:
                    goto outer;

                case TestCaseToken::ForEachDirective: {
                    if (!scan_expected(TestCaseToken::OpenParen)) {
                        goto outer;
                    }
                    for_each_arguments = scan_parameters_after_open_paren();
                    if (!scan_expected(TestCaseToken::Colon)) {
                        goto outer;
                    }
                    std::vector<std::map<std::string, std::string>*> foreach_statement = {};
                    while (true) {
                        if (!scan_optional(TestCaseToken::OpenBracket)) {
                            break;
                        }
                        auto replacement_arguments = parse_replacement_arguments();
                        if (replacement_arguments == nullptr) {
                            goto outer;
                        }
                        foreach_statement.push_back(replacement_arguments);
                    }
                    foreach_statements.push_back(foreach_statement);
                    break;
                }

                case TestCaseToken::TestDirective: {
                    std::vector<std::string> arguments;
                    if (!scan_expected(TestCaseToken::OpenParen)) {
                        goto outer;
                    }
                    arguments = scan_parameters_after_open_paren();
                    if (!scan_expected(TestCaseToken::Colon)) {
                        goto outer;
                    }
                    while (true) {
                        token = take_next_token();
                        if (token == TestCaseToken::EndOfDocument) {
                            add_diagnostic(D::Expected_end_directive_instead_reached_the_end_of_the_document);
                            break;
                        }
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
                        noexpanded_test_cases.push_back(t);
                    }
                    goto outer;
                }
                default:;
            }
        }

        outer:;

        if (foreach_statements.empty()) {
            return noexpanded_test_cases;
        }

        std::vector<TestCaseArguments*> expanded_test_cases = {};
        std::map<std::string, std::string> replacements = {};

        replace_variable_test_case(
            foreach_statements,
            noexpanded_test_cases,
            expanded_test_cases,
            replacements,
            0
        );

        return expanded_test_cases;
    }

    void
    TestCaseScanner::replace_variable_test_case(
        const std::vector<std::vector<std::map<std::string, std::string>*>>& foreach_statements,
        const std::vector<TestCaseArguments*>& noexpanded_test_cases,
        std::vector<TestCaseArguments*>& expanded_test_cases,
        std::map<std::string, std::string>& replacements,
        std::size_t index)
    {
        if (index >= foreach_statements.size()) {
            for (const auto& test_case : noexpanded_test_cases) {
                auto new_file_arguments = std::map<std::string, std::string>();
                auto new_source_code_arguments = std::map<std::string, std::string>();
                for (const auto& file_argument : test_case->file_arguments) {
                    std::string str = file_argument.second;
                    for (const auto replacement : replacements) {
                        str = boost::regex_replace(str, boost::regex("\\{\\{" + replacement.first + "\\}\\}"), replacement.second);
                    }
                    new_file_arguments.emplace(file_argument.first, str);
                }
                for (const auto& source_code_argument : test_case->source_code_arguments) {
                    std::string str = source_code_argument.second;
                    for (const auto& replacement : replacements) {
                        str = boost::regex_replace(str, boost::regex("\\{\\{" + replacement.first + "\\}\\}"), replacement.second);
                    }
                    new_source_code_arguments.emplace(source_code_argument.first, str);
                }
                expanded_test_cases.push_back(new TestCaseArguments {
                    new_file_arguments,
                    new_source_code_arguments,
                });
            }
            return;
        }

        std::map<std::string, std::string> new_replacements = {};
        for (const auto& r : replacements) {
            new_replacements.emplace(r.first, r.second);
        }
        const auto& foreach_statement = foreach_statements.at(index);
        for (const auto& foreach_arguments : foreach_statement) {
            for (const auto& foreach_argument : *foreach_arguments) {
                new_replacements.emplace(foreach_argument.first, foreach_argument.second);
            }
            replace_variable_test_case(
                foreach_statements,
                noexpanded_test_cases,
                expanded_test_cases,
                new_replacements,
                index + 1
            );
            for (const auto& foreach_argument : *foreach_arguments) {
                new_replacements.erase(foreach_argument.first);
            }
        }
    }

    std::vector<std::string>
    TestCaseScanner::scan_parameters_after_open_paren()
    {
        std::vector<std::string> parameters = {};
        while (true) {
            TestCaseToken token = take_next_token();
            if (token == TestCaseToken::EndOfDocument) {
                break;
            }
            if (token != TestCaseToken::Identifier) {
                add_diagnostic(D::Expected_identifier);
                break;
            }
            parameters.push_back(get_token_value());
            if (scan_optional(TestCaseToken::Comma)) {
                continue;
            }
            scan_expected(TestCaseToken::CloseParen);
            break;
        }
        return parameters;
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
        auto file_arguments = parse_replacement_arguments();
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
    TestCaseScanner::parse_replacement_arguments()
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
                continue;
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
            auto string_value = get_string_literal();
            std::string new_value = string_value;
            file_arguments->emplace(key, new_value);
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
                case Character::NewLine:
                    newline_position = position - 1;
                    if (!skip_white_space) {
                        return TestCaseToken::WhiteSpace;
                    }
                    column = 1;
                    token_start_column = 1;
                    token_start_line++;
                    token_start_position++;
                    continue;
                case Character::CarriageReturn:
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
                case Character::DoubleQuote:
                    return scan_string_literal();
                case Character::ByteOrderMark:
                case Character::Space:
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
                case Character::Comma:
                    return TestCaseToken::Comma;
                case Character::OpenParen:
                    return TestCaseToken::OpenParen;
                case Character::CloseParen:
                    return TestCaseToken::CloseParen;
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
                                case TestCaseToken::ForEachDirective:
                                    if (!in_definition_location) {
                                        source_code_template = source.substr(0, newline_position);
                                        in_definition_location = true;
                                    }
                                    return TestCaseToken::ForEachDirective;
                                case TestCaseToken::TestDirective:
                                    if (!in_definition_location) {
                                        source_code_template = source.substr(0, newline_position);
                                        in_definition_location = true;
                                    }
                                    return TestCaseToken::TestDirective;
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
            if (result == TestCaseToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_got_1, test_case_token_to_string.at(token), "EndOfDocument");
                return false;
            }
            if (test_case_token_to_string.find(result) == test_case_token_to_string.end()) {
                add_diagnostic(D::Expected_0_but_got_1, test_case_token_to_string.at(token), get_token_value());
                return false;
            }
            add_diagnostic(D::Expected_0_but_got_1, test_case_token_to_string.at(token), test_case_token_to_string.at(result));
            return false;
        }
        return true;
    }

    bool
    TestCaseScanner::scan_optional(const TestCaseToken& token)
    {
        return try_scan(token) == token;
    }

    TestCaseToken
    TestCaseScanner::try_scan(const TestCaseToken& token)
    {
        save();
        auto result = take_next_token();
        if (result == token) {
            return token;
        }
        revert();
        return result;
    }


    void
    TestCaseScanner::save()
    {
        saved_text_cursors.emplace(
            position,
            line,
            column,
            token_start_position,
            token_start_line,
            token_start_column
        );
    }

    void
    TestCaseScanner::revert() {
        const SavedTextCursor& saved_text_cursor = saved_text_cursors.top();
        position = saved_text_cursor.position;
        line = saved_text_cursor.line;
        column = saved_text_cursor.column;
        token_start_position = saved_text_cursor.token_start_position;
        token_start_line = saved_text_cursor.token_start_line;
        token_start_column = saved_text_cursor.token_start_column;
        saved_text_cursors.pop();
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
            if (ch == Character::Backslash) {
                scan_escape_sequence();
                continue;
            }

            if (ch == Character::DoubleQuote) {
                string_literal += source.substr(start, position - start);
                increment_position();
                break;
            }
            increment_position();
        }
        return TestCaseToken::StringLiteral;
    }


    void
    TestCaseScanner::scan_escape_sequence()
    {
        std::size_t start_position = position;
        std::size_t start_line = line;
        std::size_t start_column = column;
        increment_position();
        char32_t ch = current_char();
        switch (ch) {
            case Character::DoubleQuote:
            case Character::Backslash:
            case Character::Slash:
            case Character::b:
            case Character::f:
            case Character::n:
            case Character::r:
            case Character::t:
                increment_position();
                return;
            default:;
        }
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
        column++;
        position++;
    }
}
#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include <lib/character.h>
#include <experimental/optional>

using namespace flashpoint::program;

namespace flashpoint::program::graphql {

    GraphQlScanner::GraphQlScanner(const Glib::ustring* source):
        source(source),
        size(static_cast<unsigned int>(source->size()))
    { }

    char32_t
    GraphQlScanner::current_char() const
    {
        return (*source)[position];
    }

    unsigned long long
    GraphQlScanner::get_token_length()
    {
        return position - start_position;
    }

    GraphQlToken
    GraphQlScanner::take_next_token(bool treat_keyword_as_name)
    {
        set_token_start_position();
        while (position < size) {
            char32_t ch = current_char();
            increment_position();
            switch (ch) {
                case NewLine:
                    position_to_line_list.emplace_back(position, ++line);
                    column = 1;
                    start_column = 1;
                    start_line++;
                    start_position++;
                    continue;
                case CarriageReturn:
                    if ((*source)[position + 1] == NewLine) {
                        increment_position();
                    }
                    position_to_line_list.emplace_back(position, ++line);
                    column = 1;
                    start_line++;
                    start_column = 1;
                    start_position++;
                    continue;
                case Space:
                case Comma:
                    start_position++;
                    start_column++;
                    continue;
                case Hash:
                    start_position++;
                    start_column++;
                    while (position < size && !is_line_break(current_char())) {
                        increment_position();
                        start_position++;
                        start_column++;
                    }
                    continue;
                case Ampersand:
                    return GraphQlToken::Ampersand;
                case OpenParen:
                    return GraphQlToken::OpenParen;
                case CloseParen:
                    return GraphQlToken::CloseParen;
                case OpenBracket:
                    return GraphQlToken::OpenBracket;
                case CloseBracket:
                    return GraphQlToken::CloseBracket;
                case OpenBrace:
                    return GraphQlToken::OpenBrace;
                case CloseBrace:
                    return GraphQlToken::CloseBrace;
                case Colon:
                    return GraphQlToken::Colon;
                case Exclamation:
                    return GraphQlToken::Exclamation;
                case DoubleQuote:
                    return scan_string_literal();
                case Dot:
                    return scan_ellipses_after_first_dot();
                case Pipe:
                    return GraphQlToken::Pipe;
                case Equal:
                    return GraphQlToken::Equal;
                case Minus:
                case _0:
                case _1:
                case _2:
                case _3:
                case _4:
                case _5:
                case _6:
                case _7:
                case _8:
                case _9:
                    return scan_number();


                default:
                    if (is_name_start(ch)) {
                        std::size_t name_size = 1;
                        while (position < size && is_name_part(current_char())) {
                            increment_position();
                            name_size++;
                        }
                        if (treat_keyword_as_name) {
                            return GraphQlToken::G_Name;
                        }
                        return get_name_from_value(name_size, get_value().c_str());
                    }
                    return GraphQlToken::Unknown;
            }
        }
        return GraphQlToken::EndOfDocument;
    }

    GraphQlToken
    GraphQlScanner::scan_number()
    {
        scan_integer_part();
        switch (current_char()) {
            case Character::Dot: {
                increment_position();
                char32_t ch = current_char();
                if (!is_digit(ch)) {
                    return GraphQlToken::Unknown;
                }
                increment_position();
                scan_digit_list();
                increment_position();
                ch = current_char();
                if (ch != Character::e && ch != Character::E) {
                    return GraphQlToken::FloatLiteral;
                }
            }
            case Character::e:
            case Character::E: {
                increment_position();
                char32_t ch = current_char();
                if (ch == Character::Minus || ch == Character::Plus) {
                    increment_position();
                    ch = current_char();
                }
                scan_digit_list();
                return GraphQlToken::FloatLiteral;
            }

            default:
                return GraphQlToken::IntegerLiteral;
        }
    }

    Glib::ustring
    GraphQlScanner::get_text_from_syntax(Syntax* syntax)
    {
        return source->substr(syntax->start, syntax->end - syntax->start);
    }

    GraphQlToken
    GraphQlScanner::scan_integer_part()
    {
        auto ch = current_char();
        if (ch == Character::Minus) {
            ch = current_char();
        }
        if (ch == Character::_0) {
            increment_position();
            ch = current_char();
            if (is_digit(ch)) {
                return GraphQlToken::Unknown;
            }
            return GraphQlToken::IntegerLiteral;
        }
        else {
            if (!is_digit(ch)) {
                return GraphQlToken::Unknown;
            }
        }
        scan_digit_list();
        return GraphQlToken::IntegerLiteral;
    }

    void
    GraphQlScanner::scan_digit_list()
    {
        while (is_digit(current_char())) {
            increment_position();
        }
    }

    bool GraphQlScanner::is_digit(char32_t ch)
    {
        return ch >= Character::_0 && ch <= Character::_9;
    }

    void GraphQlScanner::skip_block()
    {
        int matched_braces = 0;
        bool encountered_brace = false;
        while (true) {
            auto token = take_next_token(false);
            switch (token) {
                case GraphQlToken::OpenBrace:
                    matched_braces++;
                    encountered_brace = true;
                    break;
                case GraphQlToken::CloseBrace:
                    matched_braces--;
                    encountered_brace = true;
                    break;
                case GraphQlToken::EndOfDocument:
                    return;
                default:;
            }
            if (encountered_brace && matched_braces <= 0) {
                return;
            }
        }
        throw std::logic_error("Should not reach here.");
    }

    GraphQlToken
    GraphQlScanner::skip_to(std::vector<GraphQlToken> tokens)
    {
        while (true) {
            save();
            GraphQlToken token = take_next_token(false);
            if (token == GraphQlToken::EndOfDocument) {
                return token;
            }
            if (std::find(tokens.begin(), tokens.end(), token) != tokens.end()) {
                revert();
                return token;
            }
        }
    }

    Glib::ustring
    GraphQlScanner::get_name() const
    {
        return name;
    }

    GraphQlToken
    GraphQlScanner::get_name_from_value(std::size_t size, const char *value)
    {
        switch (size) {
            case 2:
                switch (value[0]) {
                    case I:
                        if (strcmp(value + 1, "D") == 0) {
                            return GraphQlToken::IDKeyword;
                        }
                        break;
                    case o:
                        if (strcmp(value + 1, "n") == 0) {
                            return GraphQlToken::OnKeyword;
                        }
                        break;
                    default:;
                }
                break;
            case 3:
                if (strcmp(value, "Int") == 0) {
                    return GraphQlToken::IntKeyword;
                }
                break;
            case 4:
                switch (value[0]) {
                    case t:
                        if (strcmp(value + 1, "ype") == 0) {
                            return GraphQlToken::TypeKeyword;
                        }
                        break;
                    case n:
                        if (strcmp(value + 1, "ull") == 0) {
                            return GraphQlToken::NullKeyword;
                        }
                        break;
                    default:;
                }
                break;
            case 5:
                switch (value[0]) {
                    case q:
                        if (strcmp(value + 1, "uery") == 0) {
                            return GraphQlToken::QueryKeyword;
                        }
                        break;
                    case F:
                        if (strcmp(value + 1, "loat") == 0) {
                            return GraphQlToken::FloatKeyword;
                        }
                        break;
                    case i:
                        if (strcmp(value + 1, "nput") == 0) {
                            return GraphQlToken::InputKeyword;
                        }
                        break;
                    case u:
                        if (strcmp(value + 1, "nion") == 0) {
                            return GraphQlToken::UnionKeyword;
                        }
                        break;
                    default:;
                }
                break;
            case 6:
                switch (value[0]) {
                    case S:
                        if (strcmp(value + 1, "tring") == 0) {
                            return GraphQlToken::StringKeyword;
                        }
                        break;
                    case s:
                        if (strcmp(value + 1, "chema") == 0) {
                            return GraphQlToken::SchemaKeyword;
                        }
                        break;
                    default:;
                }
                break;
            case 7:
                if (strcmp(value, "Boolean") == 0) {
                    return GraphQlToken::BooleanKeyword;
                }
                break;
            case 8:
                switch (value[0]) {
                    case m:
                        if (strcmp(value + 1, "utation") == 0) {
                            return GraphQlToken::MutationKeyword;
                        }
                        break;
                    case f:
                        if (strcmp(value + 1, "ragment") == 0) {
                            return GraphQlToken::FragmentKeyword;
                        }
                        break;
                    default:;
                }
                break;
            case 9:
                if (strcmp(value, "interface") == 0) {
                    return GraphQlToken::InterfaceKeyword;
                }
                break;
            case 10:
                if (strcmp(value, "implements") == 0) {
                    return GraphQlToken::ImplementsKeyword;
                }
                break;
            case 12:
                if (strcmp(value, "subscription") == 0) {
                    return GraphQlToken::SubscriptionKeyword;
                }
                break;
            default:;
        }
        name = value;
        return GraphQlToken::G_Name;
    }

    void
    GraphQlScanner::set_token_start_position()
    {
        start_position = position;
        start_column = column;
    }

    Location
    GraphQlScanner::get_token_location(Syntax* syntax)
    {
        Location location = search_for_line(0, position_to_line_list.size() - 1, syntax->start);
        location.length = syntax->end - syntax->start;
        return location;
    }

    Location
    GraphQlScanner::search_for_line(std::size_t start, std::size_t end, std::size_t position)
    {
        if (end > start) {
            auto mid = start + (end - start) / 2;
            auto position_line = position_to_line_list[mid];
            if (position >= position_line.position && position < position_to_line_list[mid + 1].position) {
                return Location {
                    position_line.line,
                    position - position_line.position + 1,
                    0,
                };
            }

            if (position < position_line.position) {
                return search_for_line(start, mid - 1, position);
            }

            return search_for_line(mid + 1, end, position);
        }
        else if (end == start) {
            auto position_line = position_to_line_list[end];
            if (position >= position_line.position && position < size) {
                return Location {
                    position_line.line,
                    position - position_line.position + 1,
                    0,
                };
            }
        }

        throw std::logic_error("Could not find position: " + std::to_string(position));
    }

    GraphQlToken
    GraphQlScanner::scan_string_literal()
    {
        auto start = position;
        string_literal = "";
        while (true) {
            if (position > size) {
                break;
            }
            char32_t ch = current_char();
            if (ch == Character::Backslash) {
                string_literal += source->substr(start, position - start);
                string_literal += scan_escape_sequence();
            }
            if (ch == Character::DoubleQuote) {
                string_literal += source->substr(start, position - start);
                increment_position();
                break;
            }
            increment_position();
        }
        return GraphQlToken::StringLiteral;
    }

    Glib::ustring
    GraphQlScanner::scan_escape_sequence()
    {
        switch (current_char()) {
            case Character::DoubleQuote:
                return "\"";
            case Character::Backslash:
                return "\\";
            case Character::b:
                return "\b";
            case Character::f:
                return "\f";
            case Character::n :
                return "\n";
            case Character::r:
                return "\r";
            case Character::t:
                return "\t";
            case Character::u:
                return "\\u" + scan_hexadecimal_escape();

        }
    }

    Glib::ustring
    GraphQlScanner::scan_hexadecimal_escape()
    {
        std::size_t n = 0;
        Glib::ustring value = "";
        while (true) {
            char32_t ch = current_char();
            if (is_hexadecimal(ch)) {
                value += ch;
            }
            if (n == 4) {
                break;
            }
            n++;
        }
        return value;
    }

    bool
    GraphQlScanner::is_hexadecimal(char32_t ch) {
        return ch >= Character::_0 && ch <= Character::_9 ||
            ch >= Character::a && ch <= Character::f ||
            ch >= Character::A && ch <= Character::F;
    }

    GraphQlToken
    GraphQlScanner::scan_ellipses_after_first_dot()
    {
        char32_t ch = current_char();
        if (ch != Character::Dot) {
            return GraphQlToken::Unknown;
        }
        increment_position();
        ch = current_char();
        if (ch != Character::Dot) {
            return GraphQlToken::Unknown;
        }
        increment_position();
        return GraphQlToken::Ellipses;
    }

    bool
    GraphQlScanner::is_number(const char32_t &ch) const
    {
        return ch >= _0 && ch <= _9;
    }

    bool
    GraphQlScanner::is_name_start(const char32_t &ch) const
    {
        return (ch >= A && ch <= Z) ||
               (ch >= a && ch <= z) ||
               ch == _;
    }


    bool
    GraphQlScanner::is_name_part(const char32_t &ch) const
    {
        return (ch >= a && ch <= z) ||
               (ch >= A && ch <= Z) ||
               (ch >= _0 && ch <= _9) ||
               ch == _;
    }

    void
    GraphQlScanner::increment_position()
    {
        column++;
        position++;
    }

    GraphQlToken
    GraphQlScanner::peek_next_token()
    {
        save();
        GraphQlToken token = take_next_token(false);
        revert();
        return token;
    }

    void
    GraphQlScanner::save()
    {
        saved_text_cursors.emplace(
            position,
            line,
            column,
            start_position,
            start_line,
            start_column
        );
    }

    void
    GraphQlScanner::revert()
    {
        const SavedTextCursor& saved_text_cursor = saved_text_cursors.top();
        position = saved_text_cursor.position;
        line = saved_text_cursor.line;
        column = saved_text_cursor.column;
        start_position = saved_text_cursor.start_position;
        start_line = saved_text_cursor.start_line;
        start_column = saved_text_cursor.start_column;
        saved_text_cursors.pop();
    }

    GraphQlToken
    GraphQlScanner::try_scan(const GraphQlToken& token)
    {
        return try_scan(token, false);
    }


    GraphQlToken
    GraphQlScanner::try_scan(const GraphQlToken& token, bool treat_keyword_as_name)
    {
        save();
        auto result = take_next_token(treat_keyword_as_name);
        if (result == token) {
            return token;
        }
        revert();
        return result;
    }

    GraphQlToken
    GraphQlScanner::scan_expected(const GraphQlToken& token)
    {
        return scan_expected(token, false);
    }

    GraphQlToken
    GraphQlScanner::scan_expected(const GraphQlToken& token, bool treat_keyword_as_name)
    {
        GraphQlToken result = take_next_token(treat_keyword_as_name);
        if (result == token) {
            return token;
        }
        return result;
    }

    void
    GraphQlScanner::scan_rest_of_line()
    {
        while (position < size && !is_line_break(current_char())) {
            increment_position();
        }
    }

    inline
    bool
    GraphQlScanner::is_line_break(const char32_t& ch) const
    {
        return ch == NewLine || ch == CarriageReturn;
    }


    inline
    std::size_t
    GraphQlScanner::length() const
    {
        return position - start_position;
    }

    Glib::ustring
    GraphQlScanner::get_value() const
    {
        return source->substr(start_position, length());
    }

    Glib::ustring
    GraphQlScanner::get_string_value()
    {
        return string_literal;
    }
}

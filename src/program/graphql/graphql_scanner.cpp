#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "graphql_diagnostics.h"
#include <lib/character.h>
#include <experimental/optional>
#include <exception>

using namespace flashpoint::program;

namespace flashpoint::program::graphql {

GraphQlScanner::GraphQlScanner(const Glib::ustring& source):
    source(source),
    size(source.size())
{ }

char32_t
GraphQlScanner::current_char() const
{
    return source[position];
}

unsigned long long
GraphQlScanner::get_token_length()
{
    return position - start_position;
}

GraphQlToken
GraphQlScanner::take_next_token(bool treat_keyword_as_name, bool skip_white_space)
{
    set_token_start_position();
    while (position < size) {
        char32_t ch = current_char();
        increment_position();
        switch (ch) {
            case NewLine:
                position_to_line_list.emplace_back(position, ++line);
                if (!skip_white_space) {
                    return GraphQlToken::WhiteSpace;
                }
                column = 1;
                start_column = 1;
                start_line++;
                start_position++;
                continue;
            case CarriageReturn:
                if (source[position + 1] == NewLine) {
                    increment_position();
                }
                position_to_line_list.emplace_back(position, ++line);
                if (!skip_white_space) {
                    return GraphQlToken::WhiteSpace;
                }
                column = 1;
                start_line++;
                start_column = 1;
                start_position++;
                continue;
            case ByteOrderMark:
            case Space:
            case Comma:
                if (!skip_white_space) {
                    return GraphQlToken::WhiteSpace;
                }
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
            case At:
                return GraphQlToken::At;
            case DoubleQuote:
                return scan_string_value();
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
                    return get_token_from_value(name_size, get_value().c_str());
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
            if (!is_digit(ch)) {
                return GraphQlToken::Unknown;
            }
            increment_position();
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
    return source.substr(syntax->start, syntax->end - syntax->start);
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
        auto token = take_next_token(false, true);
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
        GraphQlToken token = take_next_token(false, true);
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
GraphQlScanner::get_token_from_value(std::size_t size, const char* value)
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
                case e:
                    if (strcmp(value + 1, "num") == 0) {
                        return GraphQlToken::EnumKeyword;
                    }
                    break;
                case t:
                    switch (value[1]) {
                        case r:
                            if (strcmp(value + 2, "ue") == 0) {
                                return GraphQlToken::TrueKeyword;
                            }
                            break;
                        case y:
                            if (strcmp(value + 2, "pe") == 0) {
                                return GraphQlToken::TypeKeyword;
                            }
                            break;
                        default:;
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
                case f:
                    if (strcmp(value + 1, "alse") == 0) {
                        return GraphQlToken::FalseKeyword;
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
            switch (value[0]) {
                case i:
                    if (strcmp(value + 1, "nterface") == 0) {
                        return GraphQlToken::InterfaceKeyword;
                    }
                    break;
                case d:
                    if (strcmp(value + 1, "irective") == 0) {
                        return GraphQlToken::DirectiveKeyword;
                    }
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

DirectiveLocation
GraphQlScanner::scan_directive_location()
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
                if (source[position + 1] == NewLine) {
                    increment_position();
                }
                position_to_line_list.emplace_back(position, ++line);
                column = 1;
                start_line++;
                start_column = 1;
                start_position++;
                continue;
            case ByteOrderMark:
            case Space:
            case Comma:
                start_position++;
                start_column++;
                continue;
            default:;
        }
        if (is_name_start(ch)) {
            std::size_t name_size = 1;
            while (position < size && is_name_part(current_char())) {
                increment_position();
                name_size++;
            }
            return get_directive_from_value(name_size, get_value().c_str());
        }
    }

    return DirectiveLocation::EndOfDocument;
}

DirectiveLocation
GraphQlScanner::get_directive_from_value(std::size_t size, const char* value)
{
    switch (size) {
        case 4:
            if (strcmp(value , "ENUM") == 0) {
                return DirectiveLocation::ENUM;
            }
            break;
        case 5:
            switch (value[0]) {
                case Q:
                    if (strcmp(value + 1, "UERY") == 0) {
                        return DirectiveLocation::QUERY;
                    }
                    break;
                case F:
                    if (strcmp(value + 1, "IELD") == 0) {
                        return DirectiveLocation::FIELD;
                    }
                    break;
                case U:
                    if (strcmp(value + 1, "NION") == 0) {
                        return DirectiveLocation::UNION;
                    }
                    break;
                default:;
            }
            break;
        case 6:
            switch (value[0]) {
                case S:
                    switch (value[2]) {
                        case H:
                            if (strcmp(value + 3, "EMA") == 0) {
                                return DirectiveLocation::SCHEMA;
                            }
                            break;

                        case A:
                            if (strcmp(value + 3, "LAR") == 0) {
                                return DirectiveLocation::SCALAR;
                            }
                            break;
                        default:;
                    }
                    break;
                case O:
                    if (strcmp(value + 1, "BJECT") == 0) {
                        return DirectiveLocation::OBJECT;
                    }
                    break;
                default:;
            }
            break;
        case 8:
            if (strcmp(value, "MUTATION") == 0) {
                return DirectiveLocation::MUTATION;
            }
            break;
        case 9:
            if (strcmp(value, "INTERFACE") == 0) {
                return DirectiveLocation::INTERFACE;
            }
            break;
        case 10:
            if (strcmp(value, "ENUM_VALUE") == 0) {
                return DirectiveLocation::ENUM_VALUE;
            }
            break;
        case 12:
            switch (value[0]) {
                case I:
                    if (strcmp(value + 1, "NPUT_OBJECT") == 0) {
                        return DirectiveLocation::INPUT_OBJECT;
                    }
                    break;
                case S:
                    if (strcmp(value + 1, "UBSCRIPTION") == 0) {
                        return DirectiveLocation::SUBSCRIPTION;
                    }
                    break;
                default:;
            }
            break;
        case 15:
            switch (value[0]) {
                case F:
                    if (strcmp(value + 1, "RAGMENT_SPREAD") == 0) {
                        return DirectiveLocation::FRAGMENT_SPREAD;
                    }
                    break;
                case I:
                    if (strcmp(value + 1, "NLINE_FRAGMENT") == 0) {
                        return DirectiveLocation::INLINE_FRAGMENT;
                    }
                    break;
                default:;
            }
            break;
        case 16:
            if (strcmp(value, "FIELD_DEFINITION") == 0) {
                return DirectiveLocation::FIELD_DEFINITION;
            }
            break;
        case 19:
            switch (value[0]) {
                case A:
                    if (strcmp(value + 1, "RGUMENT_DEFINITION") == 0) {
                        return DirectiveLocation::ARGUMENT_DEFINITION;
                    }
                    break;
                case F:
                    if (strcmp(value + 1, "RAGMENT_DEFINITION") == 0) {
                        return DirectiveLocation::FRAGMENT_DEFINITION;
                    }
                    break;
                default:;
            }
            break;
        case 22:
            if (strcmp(value, "INPUT_FIELD_DEFINITION") == 0) {
                return DirectiveLocation::INPUT_FIELD_DEFINITION;
            }
            break;
    }
    return DirectiveLocation::Unknown;
}

void
GraphQlScanner::set_token_start_position()
{
    start_position = position;
    start_column = column;
}

Location
GraphQlScanner::get_token_location(const Syntax* syntax)
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
GraphQlScanner::scan_string_value()
{
    if (current_char() == Character::DoubleQuote && source[position + 1] == Character::DoubleQuote) {
        increment_position();
        increment_position();
        return scan_string_block_after_double_quotes();
    }
    return scan_string_literal();
}

GraphQlToken
GraphQlScanner::scan_string_block_after_double_quotes()
{
    string_literal = "";
    while (true) {
        if (position > size) {
            break;
        }
        char32_t ch = current_char();
        if (ch == Character::DoubleQuote) {
            if (position + 2 >= size) {
                return GraphQlToken::Unknown;
            }
            if (source[position + 1] == DoubleQuote && source[position + 2] == DoubleQuote) {
                increment_position();
                increment_position();
                increment_position();
                return GraphQlToken::G_StringValue;
            }
        }
        if (is_source_character(ch)) {
            string_literal += ch;
            increment_position();
            continue;
        }
        increment_position();
    }
    return GraphQlToken::Unknown;
}

bool
GraphQlScanner::is_source_character(char32_t ch)
{
    switch (ch) {
        case Character::NewLine:
        case Character::CarriageReturn:
        case Character::HorizontalTab:
            return true;
        default:
            if (ch >= Character::Space && ch <= Character::End) {
                return true;
            }
    }
    return false;
}

GraphQlToken
GraphQlScanner::scan_string_literal()
{
    auto start = position;
    string_literal = "";
    std::size_t start_position = position;
    std::size_t start_line = line;
    std::size_t start_column = column;
    bool is_truncated = false;
    while (true) {
        if (position > size) {
            break;
        }
        char32_t ch = current_char();
        if (ch == Character::Backslash) {
            try {
                scan_escape_sequence();
            }
            catch (const InvalidStringException& e) {
                return GraphQlToken::InvalidString;
            }
            continue;
        }
        if (ch == Character::NewLine || ch == Character::CarriageReturn) {
            is_truncated = true;
            increment_position();
            continue;
        }
        if (ch == Character::DoubleQuote) {
            string_literal += source.substr(start, position - start);
            increment_position();
            break;
        }
        increment_position();
    }
    if (is_truncated) {
        errors.emplace(create_diagnostic(Location { start_line, start_column, position - start_position, false }, D::Unterminated_string_value));
        return GraphQlToken::InvalidString;
    }
    return GraphQlToken::G_StringValue;
}

void
GraphQlScanner::scan_escape_sequence()
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
        case Character::u:
            increment_position();
            return scan_hexadecimal_escape(start_position, start_line, start_column);
        default:;
    }
}

void
GraphQlScanner::scan_hexadecimal_escape(const std::size_t& start_position, const std::size_t& start_line, const std::size_t& start_column)
{
    std::size_t n = 0;
    Glib::ustring value = "";
    bool is_invalid = false;
    while (true) {
        char32_t ch = current_char();
        if (n == 4) {
            break;
        }
        if (is_hexadecimal(ch)) {
            value += ch;
        }
        else {
            is_invalid = true;
            errors.emplace(create_diagnostic(Location { start_line, start_column, position - start_position, false }, D::Invalid_Unicode_escape_sequence));
            break;
        }
        increment_position();
        n++;
    }
    if (is_invalid) {
        increment_position();
        throw InvalidStringException();
    }
}

bool
GraphQlScanner::is_hexadecimal(char32_t ch) {
    return (ch >= Character::_0 && ch <= Character::_9) ||
        (ch >= Character::a && ch <= Character::f) ||
        (ch >= Character::A && ch <= Character::F);
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
    GraphQlToken token = take_next_token(false, true);
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
    return try_scan(token, treat_keyword_as_name, true);
}

GraphQlToken
GraphQlScanner::try_scan(const GraphQlToken& token, bool treat_keyword_as_name, bool skip_white_space)
{
    save();
    auto result = take_next_token(treat_keyword_as_name, skip_white_space);
    if (result == token) {
        return token;
    }
    revert();
    return result;
}

GraphQlToken
GraphQlScanner::scan_expected(const GraphQlToken& token)
{
    return scan_expected(token, false, true);
}

GraphQlToken
GraphQlScanner::scan_expected(const GraphQlToken& token, bool treat_keyword_as_name)
{
    return scan_expected(token, treat_keyword_as_name, true);
}


GraphQlToken
GraphQlScanner::scan_expected(const GraphQlToken& token, bool treat_keyword_as_name, bool skip_white_space)
{
    GraphQlToken result = take_next_token(treat_keyword_as_name, skip_white_space);
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
    return source.substr(start_position, length());
}

Glib::ustring
GraphQlScanner::get_value_from_syntax(Syntax* syntax) const
{
    return source.substr(syntax->start, syntax->end);
}

Glib::ustring
GraphQlScanner::get_string_value()
{
    return string_literal;
}

Glib::ustring
GraphQlScanner::get_text_from_location(std::size_t start, std::size_t end)
{
    return source.substr(start, end - start);
}

}

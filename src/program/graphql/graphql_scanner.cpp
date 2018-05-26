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
    GraphQlScanner::take_next_token()
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
                default:
                    if (is_name_start(ch)) {
                        std::size_t name_size = 1;
                        while (position < size && is_name_part(current_char())) {
                            increment_position();
                            name_size++;
                        }
                        return get_name_from_value(name_size, get_value().c_str());
                    }
                    return GraphQlToken::Unknown;
            }
        }
        return GraphQlToken::EndOfDocument;
    }

    void GraphQlScanner::skip_block()
    {
        int matched_braces = 0;
        bool encountered_brace = false;
        while (true) {
            auto token = take_next_token();
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

    Glib::ustring
    GraphQlScanner::get_name() const
    {
        return name;
    }

    GraphQlToken
    GraphQlScanner::get_name_from_value(std::size_t size, const char *value)
    {
        // keywords are between 3 and 8 in size
        switch (size) {
            case 2:
                if (strcmp(value, "ID") == 0) {
                    return GraphQlToken::IDKeyword;
                }
                break;
            case 3:
                if (strcmp(value, "Int") == 0) {
                    return GraphQlToken::IntKeyword;
                }
                break;
            case 4:
                if (strcmp(value, "type") == 0) {
                    return GraphQlToken::TypeKeyword;
                }
                break;
            case 5:
                switch (value[0]) {
                    case q:
                        if (strcmp(value + 1, "uery") == 0) {
                            return GraphQlToken::QueryKeyword;
                        }
                    case F:
                        if (strcmp(value + 1, "loat") == 0) {
                            return GraphQlToken::FloatKeyword;
                        }
                    case i:
                        if (strcmp(value + 1, "nput") == 0) {
                            return GraphQlToken::InputKeyword;
                        }
                    default:;
                }
                break;
            case 6:
                if (strcmp(value, "String") == 0) {
                    return GraphQlToken::StringKeyword;
                }
                break;
            case 7:
                if (strcmp(value, "Boolean") == 0) {
                    return GraphQlToken::BooleanKeyword;
                }
                break;
            case 8:
                if (strcmp(value, "mutation") == 0) {
                    return GraphQlToken::MutationKeyword;
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
        Location location = binary_search_for_line(0, position_to_line_list.size() - 1, syntax->start);
        location.length = syntax->end - syntax->start;
        return location;
    }

    Location
    GraphQlScanner::binary_search_for_line(std::size_t start, std::size_t end, std::size_t position)
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
                return binary_search_for_line(start, mid - 1, position);
            }

            return binary_search_for_line(mid + 1, end, position);
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
        while (position < size && current_char() != DoubleQuote) {
            increment_position();
        }
        return GraphQlToken::Unknown;
    }

    bool
    GraphQlScanner::is_integer(const char32_t &ch) const
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
        GraphQlToken token = take_next_token();
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
        save();
        auto result = take_next_token();
        if (result == token) {
            return token;
        }
        revert();
        return result;
    }

    GraphQlToken
    GraphQlScanner::scan_expected(const GraphQlToken& token)
    {
        GraphQlToken result = take_next_token();
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

    Glib::ustring GraphQlScanner::get_value() const
    {
        return source->substr(start_position, length());
    }
}

#include "graphql_scanner.h"
#include <lib/character.h>
#include <experimental/optional>

using namespace flashpoint::program;

namespace flashpoint::program::graphql {

    GraphQlScanner::GraphQlScanner(const Glib::ustring& text):
        text(text),
        size(static_cast<unsigned int>(text.size()))
    { }

    char GraphQlScanner::current_char() const
    {
        return text[position];
    }

    GraphQlToken GraphQlScanner::next_token()
    {
        set_token_start_position();
        while (position < size) {
            char32_t ch = current_char();
            increment_position();
            switch (ch) {
                case NewLine:
                case CarriageReturn:
                case Space:
                    start_position++;
                    continue;
                case OpenBrace:
                    return GraphQlToken::OpenBrace;
                case CloseBrace:
                    return GraphQlToken::CloseBrace;
                case DoubleQuote:
                    return scan_string_literal();
                default:
                    if (is_name_start(current_char())) {
                        increment_position();
                        while (position < size && is_name_part(current_char())) {
                            increment_position();
                        }
                        return get_name_from_value(get_value().c_str());
                    }
            }
        }
        return GraphQlToken::EndOfDocument;
    }

    Glib::ustring GraphQlScanner::get_name() const
    {
        return name;
    }

    GraphQlToken GraphQlScanner::get_name_from_value(const char *value)
    {
        // keywords are between 3 and 8 in size
        switch (strlen(value)) {
            case 5:
                if (strcmp(value, "query") == 0) {
                   return GraphQlToken::QueryKeyword;
                }
            case 8:
                if (strcmp(value, "mutation") == 0) {
                    return GraphQlToken::MutationKeyword;
                }
            case 12:
                if (strcmp(value, "subscription") == 0) {
                    return GraphQlToken::SubscriptionKeyword;
                }
        }
        name = value;
        return GraphQlToken::Name;
    }

    void GraphQlScanner::set_token_start_position()
    {
        start_position = position;
    }

    GraphQlToken GraphQlScanner::scan_string_literal()
    {
        while (position < size && current_char() != DoubleQuote) {
            increment_position();
        }
        return GraphQlToken::Unknown;
    }

    bool GraphQlScanner::is_name_start(const char32_t &ch) const
    {
        return (ch >= A && ch <= Z) ||
               (ch >= a && ch <= z) ||
               ch == _;
    }


    bool GraphQlScanner::is_name_part(const char32_t &ch) const
    {
        return (ch >= a && ch <= z) ||
               (ch >= A && ch <= Z) ||
               (ch >= _0 && ch <= _9) ||
               ch == _;
    }

    void GraphQlScanner::increment_position()
    {
        position++;
    }

    GraphQlToken GraphQlScanner::peek_next_token()
    {
        save();
        GraphQlToken token = next_token();
        revert();
        return token;
    }

    void GraphQlScanner::save()
    {
        saved_text_cursors.emplace(
            position,
            start_position,
            end_position
        );
    }

    void GraphQlScanner::revert()
    {
        const SavedTextCursor& saved_text_cursor = saved_text_cursors.top();
        position = saved_text_cursor.position;
        start_position = saved_text_cursor.start_position;
        end_position = saved_text_cursor.end_position;
        saved_text_cursors.pop();
    }

    bool GraphQlScanner::try_scan(GraphQlToken token)
    {
        save();
        if (next_token() == token) {
            increment_position();
            return true;
        }
        revert();
        return false;
    }

    bool GraphQlScanner::scan_expected(GraphQlToken token)
    {
        if (next_token() == token) {
            return true;
        }
        return false;
    }

    void GraphQlScanner::scan_rest_of_line()
    {
        while (position < size && !is_line_break(current_char())) {
            increment_position();
        }
    }


    inline bool GraphQlScanner::is_line_break(const char32_t& ch) const
    {
        return ch == NewLine || ch == CarriageReturn;
    }


    inline unsigned long long GraphQlScanner::length() const
    {
        return position - start_position;
    }

    Glib::ustring GraphQlScanner::get_value() const
    {
        return text.substr(start_position, length());
    }
}

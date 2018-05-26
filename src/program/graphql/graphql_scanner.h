#ifndef FLASHPOINT_GRAPHQL_SCANNER_H
#define FLASHPOINT_GRAPHQL_SCANNER_H

#include <experimental/optional>
#include <stack>
#include <map>
#include <glibmm/ustring.h>
#include <lib/types.h>
#include "graphql_syntaxes.h"

using namespace flashpoint::lib;

namespace flashpoint::program::graphql {

    struct SavedTextCursor {
        unsigned long long position;
        unsigned long long line;
        unsigned long long column;
        unsigned long long start_position;
        unsigned long long start_line;
        unsigned long long start_column;

        SavedTextCursor(
            unsigned long long position,
            unsigned long long line,
            unsigned long long column,
            unsigned long long start_position,
            unsigned long long start_line,
            unsigned long long start_column):

            position(position),
            line(line),
            column(column),
            start_position(start_position),
            start_line(start_line),
            start_column(start_column)
        { }
    };

    enum class GraphQlToken {
        Unknown,
        G_Name,

        WhiteSpace,
        LineTerminator,
        Comment,
        G_Description,

        // Operation keywords
        MutationKeyword,
        QueryKeyword,
        SubscriptionKeyword,

        // Declaration keywords
        EnumKeyword,
        FragmentKeyword,
        InputKeyword,
        InterfaceKeyword,
        TypeKeyword,
        UnionKeyword,

        // Operator keywords
        Ampersand,
        OnKeyword,
        Pipe,
        ImplementsKeyword,

        // Scalar type keywords
        IDKeyword,
        IntKeyword,
        FloatKeyword,
        StringKeyword,
        BooleanKeyword,
        NullKeyword,

        StringLiteral,
        IntLiteral,
        FloatLiteral,
        BooleanLiteral,

        // Punctuations
        At,
        Colon,
        Comma,
        CloseBrace,
        CloseParen,
        CloseBracket,
        Dollar,
        Ellipses,
        Exclamation,
        Equal,
        OpenBrace,
        OpenBracket,
        OpenParen,

        EndOfDocument,
    };

    const std::map<const GraphQlToken, std::string> graphQlTokenToString = {
        { GraphQlToken::At, "@" },
        { GraphQlToken::Colon, ":" },
        { GraphQlToken::Equal, "=" },
        { GraphQlToken::Ellipses, "..." },
        { GraphQlToken::OpenBracket, "[" },
        { GraphQlToken::CloseBracket, "]" },
        { GraphQlToken::OpenBrace, "{" },
        { GraphQlToken::CloseBrace, "}" },
        { GraphQlToken::OpenParen, "(" },
        { GraphQlToken::CloseParen, ")" },
        { GraphQlToken::Exclamation, "!" },
        { GraphQlToken::G_Name, "name" },
        { GraphQlToken::Unknown, "unknown" },
    };

    struct PositionToLine {
        std::size_t position;
        std::size_t line;

        PositionToLine(std::size_t position, std::size_t line):
            position(position),
            line(line)
        {}
    };

    class GraphQlScanner {
    public:
        GraphQlScanner(const Glib::ustring* source);
        std::size_t length() const;
        std::size_t start_position = 0;
        std::size_t position = 0;
        std::size_t line = 1;
        std::size_t start_line = 1;
        std::size_t start_column = 1;
        std::size_t column = 1;
        std::size_t size;
        std::vector<PositionToLine> position_to_line_list = { { 0, 1 } };

        bool token_is_terminated = false;
        Glib::ustring name;

        unsigned long long
        get_token_length();

        GraphQlToken
        take_next_token();

        GraphQlToken
        try_scan(const GraphQlToken& token);

        GraphQlToken
        scan_expected(const GraphQlToken& token);

        void
        set_token_start_position();

        Location
        binary_search_for_line(std::size_t start, std::size_t end, std::size_t position);

        Location
        get_token_location(Syntax* syntax);

        void
        save();

        void
        revert();

        Glib::ustring
        get_value() const;

        Glib::ustring
        get_name() const;

        void
        scan_rest_of_line();

        void
        scan_comment_line();

        GraphQlToken
        peek_next_token();

        void
        skip_block();

    private:
        char32_t ch;
        const Glib::ustring* source;
        unsigned long long end_position;
        std::stack<SavedTextCursor> saved_text_cursors;

        char32_t
        current_char() const;

        void
        increment_position();

        bool
        is_name_start(const char32_t &ch) const;

        bool
        is_name_part(const char32_t &ch) const;

        bool
        is_integer(const char32_t &ch) const;

        GraphQlToken
        scan_string_literal();

        GraphQlToken
        get_name_from_value(std::size_t size, const char *token);

        bool
        is_line_break(const char32_t& ch) const;
    };
}


#endif //FLASHPOINT_GRAPHQL_SCANNER_H

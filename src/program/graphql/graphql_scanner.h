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

    class InvalidStringException : public std::exception {};

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
        G_Directive,

        WhiteSpace,
        LineTerminator,
        Comment,
        G_Description,

        // Operator keywords
        Ampersand,
        OnKeyword,
        Pipe,

        StartKeyword,

        // Operation keywords
        MutationKeyword,
        QueryKeyword,
        SubscriptionKeyword,

        // Declaration keywords
        DirectiveKeyword,
        EnumKeyword,
        FragmentKeyword,
        InputKeyword,
        InterfaceKeyword,
        SchemaKeyword,
        TypeKeyword,
        UnionKeyword,

        // Scalar type keywords
        IDKeyword,
        IntKeyword,
        FloatKeyword,
        StringKeyword,
        BooleanKeyword,
        NullKeyword,
        TrueKeyword,
        FalseKeyword,

        // Operator keyword
        ImplementsKeyword,

        EndKeyword,

        IntegerLiteral,
        FloatLiteral,
        BooleanLiteral,
        EnumLiteral,
        G_StringValue,
        InvalidString,
        TruncatedString,

        ObjectLiteral,

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
        Hash,
        OpenBrace,
        OpenBracket,
        OpenParen,

        EndOfDocument,
    };

    const std::map<const GraphQlToken, std::string> graphQlTokenToString = {
        { GraphQlToken::At, "@" },
        { GraphQlToken::Colon, ":" },
        { GraphQlToken::DirectiveKeyword, "directive" },
        { GraphQlToken::Equal, "=" },
        { GraphQlToken::Ellipses, "..." },
        { GraphQlToken::OpenBracket, "[" },
        { GraphQlToken::CloseBracket, "]" },
        { GraphQlToken::OpenBrace, "{" },
        { GraphQlToken::CloseBrace, "}" },
        { GraphQlToken::OpenParen, "(" },
        { GraphQlToken::CloseParen, ")" },
        { GraphQlToken::Exclamation, "!" },
        { GraphQlToken::OnKeyword, "on" },
        { GraphQlToken::Pipe, "|" },
        { GraphQlToken::G_Name, "name" },
        { GraphQlToken::G_Directive, "@directive" },
        { GraphQlToken::Unknown, "unknown" },
        { GraphQlToken::WhiteSpace, "whitespace" }
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
        GraphQlScanner(const Glib::ustring& source);
        std::size_t length() const;
        std::size_t start_position = 0;
        std::size_t end_position = 0;
        std::size_t position = 0;
        std::size_t line = 1;
        std::size_t start_line = 1;
        std::size_t start_column = 1;
        std::size_t column = 1;
        std::size_t size;
        std::vector<PositionToLine> position_to_line_list = { { 0, 1 } };
        std::stack<DiagnosticMessage> errors;
        Glib::ustring name;
        Glib::ustring directive_name;

        unsigned long long
        get_token_length();

        GraphQlToken
        take_next_token(bool treat_keyword_as_name, bool skip_white_space);

        GraphQlToken
        try_scan(const GraphQlToken& token);

        GraphQlToken
        try_scan(const GraphQlToken& token, bool treat_keyword_as_name);

        GraphQlToken
        try_scan(const GraphQlToken& token, bool treat_keyword_as_name, bool skip_white_space);

        GraphQlToken
        scan_expected(const GraphQlToken& token);

        GraphQlToken
        scan_expected(const GraphQlToken& token, bool treat_keyword_as_name);

        GraphQlToken
        scan_expected(const GraphQlToken& token, bool treat_keyword_as_name, bool skip_white_space);

        void
        set_token_start_position();

        Location
        search_for_line(std::size_t start, std::size_t end, std::size_t position);

        Location
        get_token_location(const Syntax* syntax);

        void
        save();

        void
        revert();

        Glib::ustring
        get_value() const;

        Glib::ustring
        get_value_from_syntax(Syntax* syntax) const;

        Glib::ustring
        get_name() const;

        void
        scan_rest_of_line();

        Glib::ustring
        get_text_from_syntax(Syntax*);

        void
        scan_comment_line();

        GraphQlToken
        scan_integer_part();

        bool is_digit(char32_t ch);

        GraphQlToken
        peek_next_token();

        void
        skip_block();

        GraphQlToken
        skip_to(std::vector<GraphQlToken>);

        Glib::ustring
        get_string_value();

        Glib::ustring
        get_text_from_location(std::size_t start, std::size_t end);

        DirectiveLocation
        scan_directive_location();

        DirectiveLocation
        get_directive_from_value(std::size_t size, const char* value);

    private:
        char32_t ch;

        const
        Glib::ustring&
        source;

        Glib::ustring
        string_literal;

        std::stack<SavedTextCursor>
        saved_text_cursors;

        char32_t
        current_char() const;

        void
        increment_position();

        bool
        is_name_start(const char32_t &ch) const;

        bool
        is_name_part(const char32_t &ch) const;

        bool
        is_number(const char32_t &ch) const;

        bool
        is_source_character(char32_t ch);

        GraphQlToken
        scan_string_value();

        GraphQlToken
        scan_string_literal();

        GraphQlToken
        scan_string_block_after_double_quotes();

        void
        scan_escape_sequence();

        void

        scan_hexadecimal_escape(const std::size_t& start_position, const std::size_t& start_line, const std::size_t& start_column);

        GraphQlToken
        scan_ellipses_after_first_dot();

        GraphQlToken
        get_token_from_value(std::size_t size, const char *token);

        bool
        is_line_break(const char32_t& ch) const;

        GraphQlToken
        scan_number();

        void
        scan_digit_list();

        bool
        is_hexadecimal(char32_t ch);
    };
}


#endif //FLASHPOINT_GRAPHQL_SCANNER_H

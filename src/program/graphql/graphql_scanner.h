#ifndef FLASHPOINT_GRAPHQL_SCANNER_H
#define FLASHPOINT_GRAPHQL_SCANNER_H

#include <experimental/optional>
#include <stack>
#include <glibmm/ustring.h>

namespace flashpoint::program::graphql {

    struct SavedTextCursor {
        unsigned long long position;
        unsigned long long start_position;
        unsigned long long end_position;

        SavedTextCursor(
            unsigned long long position,
            unsigned long long start_position,
            unsigned long long end_position):

            position(position),
            start_position(start_position),
            end_position(end_position)
        { }
    };

    enum class GraphQlToken {
        Unknown,
        Name,

        WhiteSpace,
        LineTerminator,
        Comment,

        // Operation keywords
        QueryKeyword,
        MutationKeyword,
        SubscriptionKeyword,

        OnKeyword,
        FragmentKeyword,
        TypeKeyword,
        UnionKeyword,
        EnumKeyword,

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
        Pipe,

        EndOfDocument,
    };

    class GraphQlScanner {
    public:
        GraphQlScanner(const Glib::ustring& text);
        unsigned long long length() const;
        unsigned long long start_position = 0;
        unsigned long long position = 0;
        unsigned long long line = 1;
        unsigned long long column = 1;
        unsigned long long start_column = 1;
        unsigned long long start_line = 1;
        unsigned int token_length;
        bool token_is_terminated = false;
        GraphQlToken next_token();
        bool try_scan(GraphQlToken token);
        bool scan_expected(GraphQlToken token);
        void set_token_start_position();
        void save();
        void revert();
        Glib::ustring get_value() const;
        Glib::ustring get_name() const;
        Glib::ustring name;
        void scan_rest_of_line();
        GraphQlToken peek_next_token();

    private:
        char32_t ch;
        const Glib::ustring& text;
        unsigned long long size;
        unsigned long long end_position;
        std::stack<SavedTextCursor> saved_text_cursors;
        char current_char() const;
        void increment_position();
        bool is_name_start(const char32_t &ch) const;
        bool is_name_part(const char32_t &ch) const;
        GraphQlToken scan_string_literal();
        GraphQlToken get_name_from_value(const char *token);
        bool is_line_break(const char32_t& ch) const;
    };
}


#endif //FLASHPOINT_GRAPHQL_SCANNER_H

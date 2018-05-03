#ifndef FLASHPOINT_GRAPHQL_SCANNER_H
#define FLASHPOINT_GRAPHQL_SCANNER_H

#include <experimental/optional>
#include <stack>
#include <glibmm/ustring.h>

namespace flashpoint::lib::graphql {

    struct SavedTextCursor {
        long long position;
        long long start_position;
        long long end_position;
    };

    enum class GraphQlToken {
        Unknown,
        Name,

        WhiteSpace,
        LineTerminator,
        Comment,

        QueryKeyword,
        MutationKeyword,
        SubscriptionKeyword,

        // Scalar values
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

        EndOfRequestPayload,
    };

    class GraphQlScanner {
    public:
        GraphQlScanner(const Glib::ustring& text);
        unsigned int length() const;
        unsigned int start_position = 0;
        unsigned int position = 0;
        unsigned int line = 1;
        unsigned int column = 1;
        unsigned int start_column = 1;
        bool token_is_terminated = false;
        GraphQlToken next_token();
        void set_token_start_position();
        void save();
        void revert();
        bool scan_optional(GraphQlToken token);
        void scan_expected(GraphQlToken token);
        Glib::ustring get_value() const;
        const char* get_name() const;

    private:
        char32_t ch;
        const Glib::ustring& text;
        long long size;
        long long end_position;
        std::stack<SavedTextCursor> saved_text_cursors;
        const char* name;
        char current_char() const;
        void increment_position();
        bool is_name_start(const char32_t &ch) const;
        bool is_name_part(const char32_t &ch) const;
        GraphQlToken scan_string_literal();
        GraphQlToken get_name_from_value(const char *token);
    };
}


#endif //FLASHPOINT_GRAPHQL_SCANNER_H

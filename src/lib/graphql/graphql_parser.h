#ifndef FLASHPOINT_GRAPHQL_PARSER_H
#define FLASHPOINT_GRAPHQL_PARSER_H

#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include <glibmm/ustring.h>

namespace flashpoint::lib::graphql {

    class GraphQlParser {
    public:
        void parse(const Glib::ustring& text);
    private:
        std::unique_ptr<GraphQlScanner> scanner;
        GraphQlToken next_token();
        Glib::ustring get_token_value() const;
        Query parse_query(const Operation&);
        Query parse_query_body(const Operation& operation, const std::experimental::optional<Signature>& signature);
        Syntax parse_primary_token(GraphQlToken token);
        Type parse_type();
        Value parse_value();
        ParameterList parse_parameter_list();
        SelectionSet parse_selection_set();
        Name parse_name();
        inline bool scan_optional(GraphQlToken token);
        inline void scan_expected(GraphQlToken token);
        template<class T, class ... Args>
        inline T create_syntax(SyntaxKind kind, Args ... args);
        inline GraphQlToken current_token();
        inline unsigned int current_position();
        template<typename S>
        inline S finish_syntax(S syntax);
    };
}



#endif //FLASHPOINT_GRAPHQL_PARSER_H

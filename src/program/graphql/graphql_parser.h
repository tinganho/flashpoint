#ifndef FLASHPOINT_GRAPHQL_PARSER_H
#define FLASHPOINT_GRAPHQL_PARSER_H

#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "program/diagnostic.h"
#include <glibmm/ustring.h>

namespace flashpoint::program::graphql {

    class GraphQlParser : public DiagnosticTrait<GraphQlParser> {
    public:
        Document* parse(const Glib::ustring& text);
        SpanLocation get_token_location();

        GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket);
    private:
        MemoryPool* memory_pool;
        MemoryPoolTicket* ticket;
        std::unique_ptr<GraphQlScanner> scanner;
        GraphQlToken next_token();
        Glib::ustring get_token_value() const;
        OperationDefinition* parse_operation_definition(const OperationType &);
        OperationDefinition* parse_operation_definition_body(const OperationType &operation);
        OperationDefinition* parse_primary_token(GraphQlToken token);
        Arguments* parse_arguments();
        Type* parse_type();
        Value* parse_value();
        VariableDefinitions* parse_variable_definitions();
        SelectionSet* parse_selection_set();
        SelectionSet* parse_selection_set_after_first_token();
        Name* parse_expected_name();
        Name* parse_expected_name(const char* token_name);
        Name* parse_optional_name();
        inline bool try_scan(GraphQlToken token);
        inline void scan_expected(GraphQlToken token);
        inline void scan_expected(GraphQlToken token, const char* token_name);
        template<class T, class ... Args>
        inline T* create_syntax(SyntaxKind kind, Args ... args);
        inline GraphQlToken current_token();
        inline unsigned int current_position();
        template<typename S>
        inline S* finish_syntax(S* syntax);

    };
}



#endif //FLASHPOINT_GRAPHQL_PARSER_H

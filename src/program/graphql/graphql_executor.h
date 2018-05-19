#ifndef FLASHPOINT_GRAPHQL_PARSER_H
#define FLASHPOINT_GRAPHQL_PARSER_H

#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "program/diagnostic.h"
#include <glibmm/ustring.h>

namespace flashpoint::program::graphql {

    class GraphQlParser : public DiagnosticTrait<GraphQlParser> {

    public:

        GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket);

        SchemaDocument*
        add_schema(const Glib::ustring* schema);

        QueryDocument*
        execute(const Glib::ustring* query);

        Location
        get_token_location();

        Location
        get_token_location_from_syntax(Syntax* syntax);

    private:
        
        MemoryPool* memory_pool;
        Glib::ustring object_name;
        MemoryPoolTicket* ticket;
        GraphQlScanner* scanner;
        std::map<Glib::ustring, Object*> object_types;
        Object* current_object_type;
        std::vector<Type*> forward_references;

        GraphQlToken
        next_token();

        Glib::ustring
        get_token_value() const;

        OperationDefinition*
        parse_operation_definition(const OperationType& operation);

        OperationDefinition*
        parse_query_primary_token(GraphQlToken token);

        Syntax*
        parse_schema_primary_token(GraphQlToken token);

        Arguments*
        parse_arguments();

        Type*
        parse_type();

        Type*
        parse_type_definition();

        Syntax*
        parse_value();

        VariableDefinitions*
        parse_variable_definitions();

        SelectionSet*
        parse_selection_set();

        SelectionSet*
        parse_selection_set_after_first_token();

        Name*
        parse_expected_name();

        Name*
        parse_expected_name(const char* token_name);

        Name*
        parse_optional_name();

        inline bool
        scan_optional(const GraphQlToken &);

        inline bool
        scan_expected(const GraphQlToken&);

        inline bool
        scan_expected(const GraphQlToken&, const char *token_name);

        void
        check_forward_references();

        Location
        get_location_from_syntax(Syntax* syntax);

        template<class T, class ... Args>
        inline T*
        create_syntax(SyntaxKind kind, Args ... args);

        inline GraphQlToken
        current_token();

        inline unsigned long long
        current_position();

        template<typename S>
        inline S*
        finish_syntax(S* syntax);

    };
}



#endif //FLASHPOINT_GRAPHQL_PARSER_H

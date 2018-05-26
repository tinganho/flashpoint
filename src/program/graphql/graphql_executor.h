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

    private:
        
        MemoryPool* memory_pool;
        Glib::ustring object_name;
        MemoryPoolTicket* ticket;
        GraphQlScanner* scanner;
        Object* current_object_type;
        std::map<Glib::ustring, Symbol*> symbols;
        std::map<Glib::ustring, Interface*> interfaces;
        std::vector<Type*> forward_type_references;
        std::vector<Name*> forward_interface_references;
        std::vector<Object*> objects_with_implementations;

        GraphQlToken
        take_next_token();

        Glib::ustring
        get_token_value() const;

        OperationDefinition*
        parse_operation_definition(const OperationType& operation);

        OperationDefinition*
        parse_query_primary_token(GraphQlToken token);

        Syntax*
        parse_schema_primary_token(GraphQlToken token);

        Object*
        parse_object();

        InputObject*
        parse_input_object();

        Interface*
        parse_interface();

        Implementations*
        parse_implementations();

        Name *
        parse_object_name(ObjectLike *object, SymbolKind);

        FieldsDefinition*
        parse_fields_definition(ObjectLike* object);

        FieldsDefinition*
        parse_fields_definition_after_open_brace(ObjectLike* object);

        InputFieldsDefinition*
        parse_input_fields_definition(InputObject* object);

        Name*
        parse_input_object_name(InputObject* object);

        Arguments*
        parse_arguments();

        Type*
        parse_type();

        Type*
        parse_type_annotation(bool in_input_location);

        Syntax*
        parse_value();

        VariableDefinitions*
        parse_variable_definitions();

        SelectionSet*
        parse_selection_set();

        SelectionSet*
        parse_selection_set_after_open_brace();

        Name*
        parse_expected_name();

        Name*
        parse_optional_name();

        ArgumentsDefinition*
        parse_arguments_definition();

        inline bool
        scan_optional(const GraphQlToken &);

        inline bool
        scan_expected(const GraphQlToken&);

        inline bool
        scan_expected(const GraphQlToken& token, DiagnosticMessageTemplate& _template);

        void
        check_forward_references();

        void
        check_object_implementations();

        Location
        get_location_from_syntax(Syntax* syntax);

        template<class T, class ... Args>
        inline T*
        create_syntax(SyntaxKind kind, Args ... args);

        Symbol*
        create_symbol(Glib::ustring* name, Declaration* declaration, SymbolKind kind);

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

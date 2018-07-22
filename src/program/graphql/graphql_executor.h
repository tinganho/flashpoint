#ifndef FLASHPOINT_GRAPHQL_EXECUTOR_H
#define FLASHPOINT_GRAPHQL_EXECUTOR_H

#include "graphql_scanner.h"
#include "graphql_schema.h"
#include "graphql_syntaxes.h"
#include "program/diagnostic.h"
#include "graphql_parser.h"
#include <glibmm/ustring.h>

namespace flashpoint::program::graphql {

class GraphQlExecutor : public GraphQlParser<GraphQlExecutor, GraphQlToken, GraphQlScanner> {

public:

    GraphQlExecutor(MemoryPool* memory_pool, MemoryPoolTicket* ticket);

    ExecutableDefinition*
    execute(const Glib::ustring& query);

    void
    add_schema(const GraphQlSchema& schema);

private:

    std::vector<const GraphQlSchema>
    schemas;

    Glib::ustring
    object_name;

    std::stack<ObjectLike*>
    current_object_types;

    FieldDefinition*
    current_field;

    InputValueDefinition*
    current_argument;

    Object*
    query;

    Object*
    mutation;

    Object*
    subscription;

    std::map<Glib::ustring, Symbol*>
    symbols;

    std::vector<std::tuple<Name*, ObjectLike*>>
    forward_fragment_references;

    std::vector<OperationDefinition*>
    operation_definitions;

    std::vector<FragmentDefinition*>
    fragment_definitions;

    std::map<Glib::ustring, FragmentDefinition*>
    fragments;

    std::set<Glib::ustring>
    duplicate_fragments;

    std::set<Glib::ustring>
    parsed_arguments;

    std::set<Glib::ustring>
    duplicate_arguments;

    std::set<Glib::ustring>
    duplicate_fields;

    FragmentDefinition*
    parse_fragment_definition_after_fragment_keyword();

    void
    check_fragment_assignment(Name* name, Object* fragment_object, Declaration* current_object_type);

    OperationDefinition*
    parse_operation_definition(const OperationType& operation);

    OperationDefinition*
    parse_operation_definition_body(OperationDefinition*);

    OperationDefinition*
    parse_operation_definition_body_after_open_brace(OperationDefinition*);

    Syntax*
    parse_executable_primary_token(GraphQlToken token);

    Arguments*
    parse_arguments(const std::map<Glib::ustring, InputValueDefinition*>& input_value_definitions, Field* field);

    Type*
    parse_type();

    Value*
    parse_value(Type*);

    BooleanValue*
    parse_boolean_value(Type* type, bool value);

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

    ObjectField*
    parse_object_field();

    Glib::ustring
    get_token_value_from_syntax(Syntax* syntax);

    inline
    void
    check_forward_fragment_references(const std::vector<FragmentDefinition*>&);

    inline std::size_t
    current_position();

    void
    skip_to_next_query_primary_token();
};

}

#endif //FLASHPOINT_GRAPHQL_EXECUTOR_H

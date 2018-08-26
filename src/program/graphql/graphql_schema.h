#ifndef FLASHPOINT_GRAPHQL_SCHEMA_H
#define FLASHPOINT_GRAPHQL_SCHEMA_H

#include <glibmm/ustring.h>
#include "graphql_parser.h"
#include <program/diagnostic.h>

namespace flashpoint::program::graphql {

class GraphQlSchema : public GraphQlParser<GraphQlSchema, GraphQlToken, GraphQlScanner> {
public:

    GraphQlSchema(const Glib::ustring& source, MemoryPool* memory_pool, MemoryPoolTicket* ticket);

    std::vector<Syntax*>
    definitions;

    std::vector<DiagnosticMessage>
    diagnostics;

    Object*
    query;

    Object*
    mutation;

    Object*
    subscription;

    std::map<Glib::ustring, Symbol*>
    symbols;

private:

    std::vector<Type*>
    forward_type_references;

    std::vector<Name*>
    forward_interface_references;

    std::map<Glib::ustring, Interface*>
    interfaces;

    std::vector<Object*>
    objects_with_implementations;

    std::vector<UnionTypeDefinition*>
    unions;

    std::set<Glib::ustring>
    duplicate_symbols;

    Symbol*
    create_symbol(Glib::ustring* name, Declaration* declaration, SymbolKind kind);

    Syntax*
    parse_primary_token(GraphQlToken token);

    EnumTypeDefinition*
    parse_enum();

    bool
    token_is_primary(GraphQlToken);

    void
    skip_to_next_primary_token();

    Schema*
    parse_schema();

    Name*
    parse_schema_field_after_colon();

    Interface*
    parse_interface_definition();

    Object*
    parse_object();

    InputObject*
    parse_input_object();

    Name*
    parse_object_name(ObjectLike *object, SymbolKind);

    Name*
    ParseInputObjectName(InputObject *object);

    Implementations*
    parse_implementations();

    UnionTypeDefinition*
    parse_union_after_union_keyword();

    std::map<Glib::ustring, FieldDefinition*>*
    parse_fields_definition(ObjectLike* object);

    std::map<Glib::ustring, InputFieldDefinition*>*
    parse_input_fields_definition(InputObject* object);

    std::map<Glib::ustring, FieldDefinition*>*
    parse_fields_definition_after_open_brace(ObjectLike* object);

    DirectiveDefinition*
    parse_directive_definition_after_directive_keyword();

    std::map<Glib::ustring, InputValueDefinition*>
    parse_arguments_definition_after_open_paren(DirectiveDefinition*);

    Type*
    parse_type_annotation(bool in_input_location, DirectiveDefinition* parent_directive_definition);

    Value*
    parse_value();

    std::map<Glib::ustring, Directive*>
    parse_directives(DirectiveLocation location, DirectiveDefinition* parent_directive_definition);

    std::map<Glib::ustring, Argument*>*
    parse_arguments_after_open_paren();

    void
    set_root_types(Schema *schema);

    void
    check_forward_references();

    void
    check_object_implementations();

    void
    check_value(Value* value, Type* type);

    void
    check_input_object(ObjectValue* value, Type* type);

    void
    check_directive_references();

    void
    check_recursive_directives(const Glib::ustring& parent_directive_definition, Type* location, Type* type);

    void
    check_union_members();

    inline
    bool
    is_valid_enum_value(GraphQlToken token);
};

enum class RootType : unsigned int {
    None,
    Query = 1 << 1,
    Mutation = 1 << 2,
    Subscription = 1 << 3,
};

inline constexpr RootType operator|(RootType a, RootType b) {
    return static_cast<RootType>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline constexpr RootType operator&(RootType a, RootType b) {
    return static_cast<RootType>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

} // flashpoint::program::graphql

#endif //FLASHPOINT_GRAPHQL_SCHEMA_H

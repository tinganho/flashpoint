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

        ExecutableDefinition*
        execute(const Glib::ustring* query);

        Location
        get_token_location();

    private:
        
        MemoryPool* memory_pool;
        Glib::ustring object_name;
        MemoryPoolTicket* ticket;
        GraphQlScanner* scanner;
        std::stack<ObjectLike*> current_object_types;
        FieldDefinition* current_field;
        InputValueDefinition* current_argument;
        std::map<Glib::ustring, Symbol*> symbols;
        std::map<Glib::ustring, Interface*> interfaces;
        std::vector<Type*> forward_type_references;
        std::vector<Name*> forward_interface_references;
        std::vector<std::tuple<Name*, ObjectLike*>> forward_fragment_references;
        std::vector<Object*> objects_with_implementations;
        std::vector<OperationDefinition*> operation_definitions;
        std::vector<FragmentDefinition*> fragment_definitions;
        std::map<Glib::ustring, FragmentDefinition*> fragments;
        std::set<Glib::ustring> duplicate_fragments;
        std::vector<Union*> unions;
        std::set<Glib::ustring> duplicate_symbols;
        std::set<Glib::ustring> parsed_arguments;
        std::set<Glib::ustring> duplicate_arguments;
        std::set<Glib::ustring> duplicate_fields;
        Glib::ustring current_description;
        bool has_description;
        GraphQlToken current_value_token;

        GraphQlToken
        take_next_token();

        GraphQlToken
        take_next_token(bool treat_keyword_as_name);

        Glib::ustring
        get_token_value() const;

        Glib::ustring
        get_string_value();

        FragmentDefinition*
        parse_fragment_definition_after_fragment_keyword();

        void
        check_fragment_assignment(Name* name, Object* fragment_object, Declaration* current_object_type);

        Schema*
        parse_schema();

        Name*
        parse_schema_field_after_colon();

        OperationDefinition*
        parse_operation_definition(const OperationType& operation);

        OperationDefinition*
        parse_operation_definition_body(OperationDefinition*);

        OperationDefinition*
        parse_operation_definition_body_after_open_brace(OperationDefinition*);

        Syntax*
        parse_executable_primary_token(GraphQlToken token);

        Syntax*
        parse_schema_primary_token(GraphQlToken token);

        Object*
        parse_object();

        InputObject*
        parse_input_object();

        Interface*
        parse_interface();

        Union*
        parse_union_after_union_keyword();

        EnumTypeDefinition*
        parse_enum();

        Implementations*
        parse_implementations();

        Name*
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

        ArgumentsDefinition*
        parse_arguments_definition();

        ObjectField*
        parse_object_field();

        inline bool
        scan_optional(const GraphQlToken &);

        inline bool
        scan_optional(const GraphQlToken &, bool treat_keyword_as_name);

        inline bool
        scan_expected(const GraphQlToken&);

        inline bool
        scan_expected(const GraphQlToken&, bool treat_keyword_as_name);

        inline bool
        scan_expected(const GraphQlToken& token, DiagnosticMessageTemplate& _template);

        inline bool
        scan_expected(const GraphQlToken& token, DiagnosticMessageTemplate& _template, bool treat_keyword_as_name);

        void
        check_forward_references();

        void
        check_object_implementations();

        void
        check_union_members();

        inline
        void
        check_forward_fragment_references(const std::vector<FragmentDefinition*>&);

        Location
        get_location_from_syntax(Syntax* syntax);

        template<class T, class ... Args>
        inline T*
        create_syntax(SyntaxKind kind, Args ... args);

        Symbol*
        create_symbol(Glib::ustring* name, Declaration* declaration, SymbolKind kind);

        inline std::size_t
        current_position();

        template<typename S>
        inline S*
        finish_syntax(S* syntax);

        Glib::ustring
        get_type_name(Type* type);

        void
        skip_to_next_schema_primary_token();

        bool
        token_is_primary(GraphQlToken);

        inline
        GraphQlToken
        skip_to(const std::vector<GraphQlToken>& tokens);

        inline
        bool
        is_valid_enum_value(GraphQlToken token);

        void
        take_errors_from_scanner();
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
}



#endif //FLASHPOINT_GRAPHQL_PARSER_H

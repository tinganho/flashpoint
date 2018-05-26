#include "graphql_executor.h"
//#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "graphql_diagnostics.h"
#include <memory>

namespace flashpoint::program::graphql {

    GraphQlParser::GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket):
        memory_pool(memory_pool),
        ticket(ticket)
    { }

    SchemaDocument* GraphQlParser::add_schema(const Glib::ustring *schema)
    {
        scanner = new GraphQlScanner(schema);
        auto schema_document = create_syntax<SchemaDocument>(SyntaxKind::S_QueryDocument);
        schema_document->source = schema;
        GraphQlToken token = take_next_token();
        while (token != GraphQlToken::EndOfDocument) {
            auto type_definition = parse_schema_primary_token(token);
            if (type_definition != nullptr) {
                schema_document->definitions.push_back(type_definition);
            }
            else {
                scanner->skip_block();
            }
            token = take_next_token();
        }
        check_forward_references();
        check_object_implementations();
        if (!diagnostics.empty()) {
            schema_document->diagnostics = diagnostics;
        }
        return schema_document;
    }

    void GraphQlParser::check_forward_references()
    {
        for (const auto& reference : forward_type_references) {
            auto it = symbols.find(reference->name->identifier);
            if (it == symbols.end()) {
                add_diagnostic(get_location_from_syntax(reference->name), D::Type_0_is_not_defined, reference->name->identifier);
            }
            else {
                if ((it->second->kind & SymbolKind::Input) > SymbolKind::SL_None && !reference->in_input_location) {
                    add_diagnostic(get_location_from_syntax(reference->name), D::Cannot_add_an_input_type_to_an_output_location, reference->name->identifier);

                }
                if ((it->second->kind & SymbolKind::Output) > SymbolKind::SL_None && reference->in_input_location) {
                    add_diagnostic(get_location_from_syntax(reference->name), D::Cannot_add_an_output_type_to_an_input_location, reference->name->identifier);
                }
            }
        }
    }

    void GraphQlParser::check_object_implementations()
    {
        for (const auto object : objects_with_implementations) {
            for (const auto& implementationIt : object->implementations->implementations) {
                auto interfaceIt = interfaces.find(implementationIt.first);
                if (interfaceIt == interfaces.end()) {
                    add_diagnostic(get_location_from_syntax(implementationIt.second), D::Interface_0_is_not_defined, implementationIt.first);
                }
                else {
                    for (const auto& fieldIt : interfaceIt->second->fields) {
                        if (object->fields.find(fieldIt.second->name->identifier) == object->fields.end()) {
                            add_diagnostic(get_location_from_syntax(object->name),
                                 D::The_field_0_in_interface_1_is_not_implemented,
                                 fieldIt.second->name->identifier,
                                 implementationIt.first);
                        }
                    }
                }
            }
        }
    }

    QueryDocument*
    GraphQlParser::execute(const Glib::ustring* query)
    {
        scanner = new GraphQlScanner(query);
        GraphQlToken token = take_next_token();
        auto query_document = create_syntax<QueryDocument>(SyntaxKind::S_QueryDocument);
        query_document->source = query;
        while (token != GraphQlToken::EndOfDocument) {
            auto operation_definition = parse_query_primary_token(token);
            if (operation_definition == nullptr) {
                break;
            }
            query_document->definitions.push_back(operation_definition);
            token = take_next_token();
        }
        if (!diagnostics.empty()) {
            query_document->diagnostics = diagnostics;
        }
        return query_document;
    }

    Object*
    GraphQlParser::parse_object()
    {
        auto object = create_syntax<Object>(SyntaxKind::S_Object);
        object->name = parse_object_name(object, SymbolKind::SL_Object);
        if (object->name == nullptr) {
            return nullptr;
        }
        if (scan_optional(GraphQlToken::ImplementsKeyword)) {
            object->implementations = parse_implementations();
            object->fields_definition = parse_fields_definition_after_open_brace(object);
            objects_with_implementations.push_back(object);
        }
        else {
            object->fields_definition = parse_fields_definition(object);
        }
        if (object->fields_definition == nullptr) {
            return nullptr;
        }
        return object;
    }

    Implementations*
    GraphQlParser::parse_implementations()
    {
        auto implementations = create_syntax<Implementations>(SyntaxKind::S_Implementations);
        while (true) {
            GraphQlToken token = take_next_token();
            if (token == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Unexpected_end_of_implementations);
                return nullptr;
            }
            if (token != GraphQlToken::G_Name) {
                add_diagnostic(D::Expected_name_of_an_interface);
                return nullptr;
            }
            auto token_value = get_token_value();
            auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
            implementations->implementations.emplace(token_value, name);
            auto it = symbols.find(token_value);
            if (it == symbols.end()) {
                forward_interface_references.push_back(name);
            }
            else {
                if (it->second->declaration->kind != SyntaxKind::S_Interface) {
                    add_diagnostic(D::Expected_name_of_an_interface);
                }
            }
            GraphQlToken post_token = take_next_token();
            if (post_token == GraphQlToken::OpenBrace) {
                break;
            }
            if (post_token == GraphQlToken::Ampersand) {
                continue;
            }
        }
        return implementations;
    }

    InputObject*
    GraphQlParser::parse_input_object()
    {
        auto input_object = create_syntax<InputObject>(SyntaxKind::S_InputObject);
        input_object->name = parse_input_object_name(input_object);
        if (input_object->name == nullptr) {
            return nullptr;
        }
        input_object->fields_definition = parse_input_fields_definition(input_object);
        if (input_object->fields_definition == nullptr) {
            return nullptr;
        }
        return input_object;
    }

    Interface*
    GraphQlParser::parse_interface()
    {
        auto interface = create_syntax<Interface>(SyntaxKind::S_Interface);
        interface->name = parse_object_name(interface, SymbolKind::SL_Interface);
        interface->fields_definition = parse_fields_definition(interface);
        interfaces.emplace(interface->name->identifier, interface);
        return interface;
    }

    Syntax*
    GraphQlParser::parse_schema_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::InputKeyword:
                return parse_input_object();
            case GraphQlToken::TypeKeyword:
                return parse_object();
            case GraphQlToken::InterfaceKeyword:
                return parse_interface();
            default:
                add_diagnostic(D::Expected_type_definition);
                return nullptr;
        }
    }

    Name *
    GraphQlParser::parse_object_name(ObjectLike *object, SymbolKind kind)
    {
        if (!scan_expected(GraphQlToken::G_Name, D::Expected_type_name_instead_got_0)) {
            return nullptr;
        }
        auto token_value = get_token_value();
        auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
        const auto& result = symbols.find(token_value);
        if (result == symbols.end()) {
            symbols.emplace(token_value, create_symbol(&name->identifier, object, kind));
        }
        else {
            add_diagnostic(get_location_from_syntax(result->second->declaration->name), D::Duplicate_type_0, *result->second->name);
            add_diagnostic(get_location_from_syntax(name), D::Duplicate_type_0, token_value);
        }
        return name;
    }

    Name*
    GraphQlParser::parse_input_object_name(InputObject* object)
    {
        if (!scan_expected(GraphQlToken::G_Name, D::Expected_type_name_instead_got_0)) {
            return nullptr;
        }
        auto token_value = get_token_value();
        auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
        const auto& result = symbols.find(token_value);
        if (result == symbols.end()) {
            symbols.emplace(token_value, create_symbol(&name->identifier, object, SymbolKind::SL_InputObject));
        }
        else {
            add_diagnostic(get_location_from_syntax(result->second->declaration->name), D::Duplicate_type_0, *result->second->name);
            add_diagnostic(get_location_from_syntax(name), D::Duplicate_type_0, token_value);
        }
        return name;
    }

    FieldsDefinition*
    GraphQlParser::parse_fields_definition(ObjectLike* object)
    {
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            return nullptr;
        }
        return parse_fields_definition_after_open_brace(object);
    }

    FieldsDefinition*
    GraphQlParser::parse_fields_definition_after_open_brace(ObjectLike *object)
    {
        auto fields_definition = create_syntax<FieldsDefinition>(SyntaxKind::S_FieldsDefinition);
        bool has_at_least_one_field = false;
        while (true) {
            auto field_definition = create_syntax<FieldDefinition>(SyntaxKind::S_FieldDefinition);
            GraphQlToken field_token = take_next_token();
            if (field_token == GraphQlToken::EndOfDocument) {
                return nullptr;
            }
            if (field_token == GraphQlToken::CloseBrace && has_at_least_one_field) {
                break;
            }
            if (field_token != GraphQlToken::G_Name) {
                add_diagnostic(D::Expected_field_definition);
                return nullptr;
            }
            auto name = get_token_value();
            field_definition->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (scan_optional(GraphQlToken::OpenParen)) {
                field_definition->arguments_definition = parse_arguments_definition();
            }
            if (!scan_expected(GraphQlToken::Colon)) {
                return nullptr;
            }
            field_definition->type = parse_type_annotation(/*is_input*/ false);
            if (field_definition->type == nullptr) {
                return nullptr;
            }
            fields_definition->field_definitions.push_back(field_definition);
            object->fields.emplace(name, field_definition);
            has_at_least_one_field = true;
        }
        return fields_definition;
    }

    InputFieldsDefinition*
    GraphQlParser::parse_input_fields_definition(InputObject* object)
    {
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            return nullptr;
        }
        auto input_fields_definition = create_syntax<FieldsDefinition>(SyntaxKind::S_InputFieldsDefinition);

        bool has_at_least_one_field = false;
        while (true) {
            auto input_field_definition = create_syntax<FieldDefinition>(SyntaxKind::S_InputFieldDefinition);
            GraphQlToken field_token = take_next_token();
            if (field_token == GraphQlToken::EndOfDocument) {
                return nullptr;
            }
            if (field_token == GraphQlToken::CloseBrace && has_at_least_one_field) {
                break;
            }
            if (field_token != GraphQlToken::G_Name) {
                add_diagnostic(D::Expected_field_definition);
                return nullptr;
            }
            auto name = get_token_value();
            input_field_definition->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (!scan_expected(GraphQlToken::Colon)) {
                return nullptr;
            }
            input_field_definition->type = parse_type_annotation(/*is_input*/ true);
            if (input_field_definition->type == nullptr) {
                return nullptr;
            }
            input_fields_definition->field_definitions.push_back(input_field_definition);
            object->fields.emplace(name, input_field_definition->type);
            has_at_least_one_field = true;
        }
    }

    ArgumentsDefinition*
    GraphQlParser::parse_arguments_definition()
    {
        auto arguments_definition = create_syntax<ArgumentsDefinition>(SyntaxKind::S_ArgumentsDefinition);
        do {
            GraphQlToken token = take_next_token();
            auto input_value_definition = create_syntax<InputValueDefinition>(SyntaxKind::S_InputValueDefinition);
            if (token == GraphQlToken::CloseParen) {
                break;
            }
            if (token == GraphQlToken::EndOfDocument) {
                return nullptr;
            }
            if (token != GraphQlToken::G_Name) {
                add_diagnostic(D::Expected_name_of_argument);
                return nullptr;
            }
            input_value_definition->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
            if (!scan_expected(GraphQlToken::Colon)) {
                return nullptr;
            }
            input_value_definition->type = parse_type_annotation(/*is_input*/ true);
        }
        while (true);
        return arguments_definition;
    }

    Type*
    GraphQlParser::parse_type_annotation(bool in_input_location)
    {
        GraphQlToken token = take_next_token();
        auto type = create_syntax<Type>(SyntaxKind::S_Type);
        type->in_input_location = in_input_location;
        if (token == GraphQlToken::OpenBracket) {
            type->is_list_type = true;
            token = take_next_token();
        }
        switch(token) {
            case GraphQlToken::IntKeyword:
                type->type = TypeEnum::T_Int;
                break;
            case GraphQlToken::FloatKeyword:
                type->type = TypeEnum::T_Float;
                break;
            case GraphQlToken::StringKeyword:
                type->type = TypeEnum::T_String;
                break;
            case GraphQlToken::BooleanKeyword:
                type->type = TypeEnum::T_Boolean;
                break;
            case GraphQlToken::IDKeyword:
                type->type = TypeEnum::T_ID;
                break;
            case GraphQlToken::G_Name: {
                type->type = TypeEnum::T_Object;
                auto token_value = get_token_value();
                type->name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
                auto it = symbols.find(token_value);
                type->in_input_location = in_input_location;
                if (it == symbols.end()) {
                    forward_type_references.push_back(type);
                }
                else {
                    if (in_input_location) {
                        if ((it->second->kind & SymbolKind::Output) > SymbolKind::SL_None) {
                            add_diagnostic(D::Cannot_add_an_output_type_to_an_input_location);
                        }
                    }
                    else {
                        if ((it->second->kind & SymbolKind::Input) > SymbolKind::SL_None) {
                            add_diagnostic(D::Cannot_add_an_input_type_to_an_output_location);
                        }
                        else if (it->second->kind == SymbolKind::SL_Interface) {
                            add_diagnostic(D::Cannot_add_an_interface_in_type_location);
                        }
                    }
                }
                break;
            }
            default:
                add_diagnostic(D::Expected_type_annotation_instead_got_0, get_token_value());
                return nullptr;
        }
        type->is_non_null = scan_optional(GraphQlToken::Exclamation);
        if (type->is_list_type) {
            if (!scan_expected(GraphQlToken::CloseBracket)) {
                return nullptr;
            }
            type->is_non_null_list = scan_optional(GraphQlToken::Exclamation);
        }
        return finish_syntax(type);
    }

    OperationDefinition* GraphQlParser::parse_query_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::QueryKeyword: {
                auto query = symbols.find("Query");
                if (query == symbols.end()) {
                    return nullptr;
                }
                current_object_type = static_cast<Object*>(query->second->declaration);
                return parse_operation_definition(OperationType::Query);
            }
            case GraphQlToken::MutationKeyword: {
                auto query = symbols.find("Mutation");
                if (query == symbols.end()) {
                    return nullptr;
                }
                current_object_type = static_cast<Object*>(query->second->declaration);
                return parse_operation_definition(OperationType::Mutation);
            }
            case GraphQlToken::SubscriptionKeyword: {
                auto query = symbols.find("Subscription");
                if (query == symbols.end()) {
                    return nullptr;
                }
                current_object_type = static_cast<Object*>(query->second->declaration);
                return parse_operation_definition(OperationType::Subscription);
            }
            case GraphQlToken::OpenBrace: {
                auto query = symbols.find("Query");
                if (query == symbols.end()) {
                    return nullptr;
                }
                current_object_type = static_cast<Object*>(query->second->declaration);
                return parse_operation_definition(OperationType::Query);
            }
            default:
                add_diagnostic(D::Expected_operation_definition_or_fragment_definition);
                return nullptr;
        }
    }

    OperationDefinition* GraphQlParser::parse_operation_definition(const OperationType& operation)
    {
        auto operation_definition = create_syntax<OperationDefinition>(SyntaxKind::S_OperationDefinition);
        if (scan_optional(GraphQlToken::G_Name)) {
            operation_definition->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value().c_str());
            if (scan_optional(GraphQlToken::OpenParen)) {
                operation_definition->variable_definitions = parse_variable_definitions();
                scan_expected(GraphQlToken::CloseParen);
            }
        }
        operation_definition->selection_set = parse_selection_set();
        if (operation_definition->selection_set == nullptr) {
            return nullptr;
        }
        return operation_definition;
    }

    SelectionSet* GraphQlParser::parse_selection_set()
    {
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            return nullptr;
        }
        return parse_selection_set_after_open_brace();
    }

    SelectionSet* GraphQlParser::parse_selection_set_after_open_brace()
    {
        auto selection_set = create_syntax<SelectionSet>(SyntaxKind::S_SelectionSet);
        while (true) {
            auto selection = create_syntax<Selection>(SyntaxKind::S_Selection);
            auto field = create_syntax<QueryField>(SyntaxKind::S_Field);
            GraphQlToken token = take_next_token();
            switch (token) {
                case GraphQlToken::Ellipses:
                    if (scan_optional(GraphQlToken::OnKeyword)) {
                    }
                    else {
                    }
                    break;

                case GraphQlToken::G_Name: {
                    auto token_value = get_token_value();
                    auto& fields = current_object_type->fields;
                    auto result = fields.find(token_value);
                    if (result == fields.end()) {
                        add_diagnostic(D::Field_0_doesnt_exist_on_type_1, token_value, current_object_type->name->identifier);
                        return nullptr;
                    }
                    auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
                    if (scan_optional(GraphQlToken::Colon)) {
                        field->alias = name;
                        field->name = parse_expected_name();
                    }
                    else {
                        field->name = name;
                    }
                    if (scan_optional(GraphQlToken::OpenParen)) {
                        field->arguments = parse_arguments();
                    }
                    if (scan_optional(GraphQlToken::OpenBrace)) {
                        field->selection_set = parse_selection_set_after_open_brace();
                    }
                    break;
                }

                case GraphQlToken::CloseBrace:
                    goto outer;

                case GraphQlToken::EndOfDocument: {
                    if (selection_set->selections.size() == 0) {
                        add_diagnostic({ 0, 0, 0, true }, D::Expected_at_least_a_field_inline_fragment_or_fragment_spread);
                    }
                    else {
                        add_diagnostic({ 0, 0, 0, true }, D::Unexpected_end_of_selection_set_Missing_closing_brace_0_for_selection_set, "}");
                    }
                    return nullptr;
                }

                default:
                    add_diagnostic(D::Expected_field_inline_fragment_or_fragment_spread);
                    goto outer;
            }
            selection->field = field;
            selection_set->selections.push_back(selection);
        }
        outer:
        finish_syntax(selection_set);
        return selection_set;
    }

    Arguments* GraphQlParser::parse_arguments()
    {
        auto arguments = create_syntax<Arguments>(SyntaxKind::S_Arguments);
        while (true) {
            auto name = parse_optional_name();
            if (name != nullptr) {
                break;
            }
            auto argument = create_syntax<Argument>(SyntaxKind::S_Argument);
            argument->name = name;
            argument->value = parse_value();
        }
        auto name = parse_expected_name();
        return arguments;
    }

    Name* GraphQlParser::parse_optional_name()
    {
        if (scan_optional(GraphQlToken::G_Name)) {
            return create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
        }
        return nullptr;
    }

    VariableDefinitions* GraphQlParser::parse_variable_definitions()
    {
        auto variable_definitions = create_syntax<VariableDefinitions>(SyntaxKind::S_VariableDefinitions);
        while (true) {
            auto variable_definition = create_syntax<VariableDefinition>(SyntaxKind::S_VariableDefinition);
            GraphQlToken token = take_next_token();
            if (token != GraphQlToken::G_Name) {
                break;
            }
            variable_definition->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value().c_str());
            scan_expected(GraphQlToken::Colon);
            variable_definition->type = parse_type();
            if (scan_optional(GraphQlToken::Equal)) {
                variable_definition->default_value = parse_value();
            }
            if (!scan_optional(GraphQlToken::Comma)) {
                break;
            }
            variable_definitions->variable_definitions.push_back(variable_definition);
        }
        return finish_syntax(variable_definitions);
    }

    Syntax* GraphQlParser::parse_value()
    {
        switch (take_next_token()) {
            case GraphQlToken::StringLiteral:
                return create_syntax<StringLiteral>(SyntaxKind::S_StringLiteral, get_token_value());
            case GraphQlToken::IntLiteral:
                return create_syntax<IntLiteral>(SyntaxKind::S_IntLiteral, std::atoi(get_token_value().c_str()));
            case GraphQlToken::FloatLiteral:
                return create_syntax<FloatLiteral>(SyntaxKind::S_FloatLiteral, std::atof(get_token_value().c_str()));
            case GraphQlToken::NullKeyword:
                return create_syntax<NullLiteral>(SyntaxKind::S_NullLiteral);
            case GraphQlToken::BooleanLiteral:
                return create_syntax<BooleanLiteral>(SyntaxKind::S_BooleanLiteral, get_token_value().at(0) == 't');
//            case GraphQlToken::Name:
//                return Value {
//                    ValueType::Enum,
//                    create_syntax<EnumLiteral>(SyntaxKind::EnumLiteral, get_token_value().c_str()),
//                };
//            case GraphQlToken::Dollar:
////                ArgumentLiteral argument_literal = create_syntax<ArgumentLiteral>(SyntaxKind::ArgumentLiteral);
////                argument_literal.name = parse_name();
////                finish_syntax(argument_literal);
////                return std::make_pair(argument_literal, ValueKind::Argument);
//            case GraphQlToken::OpenBrace:
//                scan_expected(GraphQlToken::CloseBrace);
//            default:
//                throw std::logic_error("Should not reach here.");
        }
    }

    Location GraphQlParser::get_token_location()
    {
        return Location {
            scanner->line,
            scanner->start_column,
            scanner->get_token_length(),
        };
    }

    Location GraphQlParser::get_location_from_syntax(Syntax* syntax)
    {
        return scanner->get_token_location(syntax);
    }

    Name* GraphQlParser::parse_expected_name()
    {
        if (!scan_expected(GraphQlToken::G_Name)) {
            return nullptr;
        }
        return create_syntax<Name>(SyntaxKind::S_Name, scanner->get_value().c_str());
    }

    Type* GraphQlParser::parse_type()
    {
        TypeEnum type;
        bool is_list_type = scan_optional(GraphQlToken::OpenBracket);
        bool is_non_null_list = false;
        switch (take_next_token()) {
            case GraphQlToken::BooleanKeyword:
                type = TypeEnum::T_Boolean;
                break;
            case GraphQlToken::IntKeyword:
                type = TypeEnum::T_Int;
                break;
            case GraphQlToken::FloatKeyword:
                type = TypeEnum::T_Float;
                break;
            case GraphQlToken::StringKeyword:
                type = TypeEnum::T_String;
                break;
            case GraphQlToken::IDKeyword:
                type = TypeEnum::T_ID;
                break;
            default:
                type = TypeEnum::T_Object;
        }
        bool is_non_null = scan_optional(GraphQlToken::Exclamation);
        if (is_list_type) {
            scan_expected(GraphQlToken::CloseBracket);
            is_non_null_list = scan_optional(GraphQlToken::Exclamation);
        }
        return create_syntax<Type>(SyntaxKind::S_Type, type, is_non_null, is_list_type, is_non_null_list, false);
    }

    unsigned long long GraphQlParser::current_position()
    {
        return scanner->position;
    }

    inline bool GraphQlParser::scan_optional(const GraphQlToken &token)
    {
        return scanner->try_scan(token) == token;
    }

    inline bool GraphQlParser::scan_expected(const GraphQlToken& token)
    {
        GraphQlToken result = scanner->scan_expected(token);
        if (result != token) {
            add_diagnostic(D::Expected_0_but_got_1, graphQlTokenToString.at(token), get_token_value());
            return false;
        }
        return true;
    }

    inline bool GraphQlParser::scan_expected(const GraphQlToken& token, DiagnosticMessageTemplate& _template)
    {
        GraphQlToken result = scanner->scan_expected(token);
        if (result != token) {
            add_diagnostic(_template, get_token_value());
            return false;
        }
        return true;
    }

    template<typename T, typename ... Args>
    T* GraphQlParser::create_syntax(SyntaxKind kind, Args ... args) {
        return new (memory_pool, ticket) T (kind, scanner->start_position, scanner->position, args...);
    }

    Symbol*
    GraphQlParser::create_symbol(Glib::ustring* name, Declaration* declaration, SymbolKind kind)
    {
        return new (memory_pool, ticket) Symbol(name, declaration, kind);
    }

    template<typename S>
    inline S* GraphQlParser::finish_syntax(S* syntax)
    {
        syntax->end = current_position() + 1;
        return syntax;
    }

    inline GraphQlToken GraphQlParser::take_next_token()
    {
        return scanner->take_next_token();
    }

    inline Glib::ustring GraphQlParser::get_token_value() const
    {
        return scanner->get_value();
    }

    inline GraphQlToken GraphQlParser::current_token() {
        return scanner->take_next_token();
    }
}
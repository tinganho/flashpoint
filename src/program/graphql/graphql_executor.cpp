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
        bool has_schema_query_root_operation = false;
        while (token != GraphQlToken::EndOfDocument) {
            auto type_definition = parse_schema_primary_token(token);
            if (type_definition != nullptr) {
                schema_document->definitions.push_back(type_definition);
                if (type_definition->kind == SyntaxKind::S_Schema) {
                    if (static_cast<Schema*>(type_definition)->query != nullptr) {
                        has_schema_query_root_operation = true;
                    }
                    break;
                }
            }
            else {
                scanner->skip_block();
            }
            token = take_next_token();
        }
        if (!has_schema_query_root_operation) {
            const auto& symbols_it = symbols.find("Query");
            if (symbols_it == symbols.end()) {
                add_diagnostic({ 0, 0, 0, true }, D::No_query_root_operation_type_defined);
            }
        }
        check_forward_references();
        check_object_implementations();
        check_union_members();
        if (!diagnostics.empty()) {
            schema_document->diagnostics = diagnostics;
        }
        finish_syntax(schema_document);
        return schema_document;
    }

    ExecutableDefinition*
    GraphQlParser::execute(const Glib::ustring* query)
    {
        scanner = new GraphQlScanner(query);
        GraphQlToken token = take_next_token();
        auto executable_definition = create_syntax<ExecutableDefinition>(SyntaxKind::S_QueryDocument);
        executable_definition->source = query;
        while (token != GraphQlToken::EndOfDocument) {
            auto definition = parse_executable_primary_token(token);
            if (definition == nullptr) {
                break;
            }
            if (definition->kind == SyntaxKind::S_FragmentDefinition) {
                executable_definition->fragment_definitions.push_back(static_cast<FragmentDefinition*>(definition));
            }
            else {
                executable_definition->operation_definitions.push_back(static_cast<OperationDefinition*>(definition));
            }
            token = take_next_token();
        }
        check_forward_fragment_references(executable_definition->fragment_definitions);
        if (!diagnostics.empty()) {
            executable_definition->diagnostics = diagnostics;
        }
        return executable_definition;
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
                    add_diagnostic(
                        get_location_from_syntax(implementationIt.second),
                        D::Interface_0_is_not_defined,
                        implementationIt.first
                    );
                }
                else {
                    for (const auto& interfaceFieldIt : interfaceIt->second->fields) {
                        const auto& objectFieldIt = object->fields.find(interfaceFieldIt.second->name->identifier);
                        if (objectFieldIt == object->fields.end()) {
                            add_diagnostic(get_location_from_syntax(object->name),
                                 D::The_field_0_in_interface_1_is_not_implemented,
                                 interfaceFieldIt.second->name->identifier,
                                 implementationIt.first);
                        }
                        else {
                            const auto& objectType = objectFieldIt->second->type;
                            const auto& interfaceType = interfaceFieldIt.second->type;
                            if (objectType->type != interfaceType->type) {
                                add_diagnostic(
                                    get_location_from_syntax(objectType),
                                    D::Type_0_does_not_match_type_1_from_the_interface_2,
                                    get_type_name(objectType),
                                    get_type_name(interfaceType),
                                    interfaceIt->second->name->identifier
                                );
                                break;
                            }
                            if ((objectType->type & TypeEnum::T_SymbolicType) != TypeEnum::T_None) {
                                if (objectType->name->identifier != interfaceType->name->identifier) {
                                    add_diagnostic(
                                        get_location_from_syntax(objectType),
                                        D::Type_0_does_not_match_type_1_from_the_interface_2,
                                        get_type_name(objectType),
                                        get_type_name(interfaceType),
                                        interfaceIt->second->name->identifier
                                    );
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void
    GraphQlParser::check_union_members()
    {
        for (const auto& _union : unions) {
            for (const auto& membersIt : _union->members) {
                const auto& name = membersIt.second;
                const auto& symbolsIt = symbols.find(name->identifier);
                if (symbolsIt == symbols.end()) {
                    add_diagnostic(
                        get_location_from_syntax(name),
                        D::Type_0_is_not_defined,
                        name->identifier);
                    continue;
                }
                if (symbolsIt->second->kind != SymbolKind::SL_Object) {
                    add_diagnostic(get_location_from_syntax(name), D::Only_objects_are_allowed_as_members_in_a_union);
                }
            }
        }
    }

    inline
    void
    GraphQlParser::check_forward_fragment_references(const std::vector<FragmentDefinition*>& fragment_definitions)
    {
        for (const auto& forward_fragment_reference : forward_fragment_references) {
            const auto& name = std::get<0>(forward_fragment_reference);
            FragmentDefinition* current_fragment_definition = nullptr;
            for (const auto& fragment_definition : fragment_definitions) {
                if (name->identifier == fragment_definition->name->identifier) {
                    current_fragment_definition = fragment_definition;
                    break;
                }
            }
            if (current_fragment_definition == nullptr) {
                add_diagnostic(
                    get_location_from_syntax(name),
                    D::Fragment_0_is_not_defined,
                    name->identifier
                );
                continue;
            }
            const auto& current_object_type = std::get<1>(forward_fragment_reference);
            auto symbolsIt = symbols.find(current_fragment_definition->type->identifier);
            check_fragment_assignment(name, static_cast<Object*>(symbolsIt->second->declaration), current_object_type);
        }
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
            if (object->implementations == nullptr) {
                return nullptr;
            }
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
                add_diagnostic(D::Expected_name_of_interface);
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
                    add_diagnostic(D::Expected_name_of_interface);
                }
            }
            GraphQlToken post_token = take_next_token();
            if (post_token == GraphQlToken::OpenBrace) {
                break;
            }
            if (post_token == GraphQlToken::Ampersand) {
                continue;
            }
            add_diagnostic(D::Expected_0_but_got_1, "{", get_token_value());
            scanner->skip_block();
            return nullptr;
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
        if (interface->name == nullptr) {
            return nullptr;
        }
        interface->fields_definition = parse_fields_definition(interface);
        interfaces.emplace(interface->name->identifier, interface);
        return interface;
    }

    Union*
    GraphQlParser::parse_union_after_union_keyword()
    {
        auto _union = create_syntax<Union>(SyntaxKind::S_Union);
        if (!scan_expected(GraphQlToken::G_Name)) {
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        auto token_value = get_token_value();
        _union->name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
        if (!scan_expected(GraphQlToken::Equal)) {
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        scan_optional(GraphQlToken::Pipe);
        if (!scan_expected(GraphQlToken::G_Name)) {
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        auto member_name = get_token_value();
        _union->members.emplace(member_name ,create_syntax<Name>(SyntaxKind::S_Name, get_token_value()));
        while (true) {
            scanner->save();
            GraphQlToken token = take_next_token();
            if (token != GraphQlToken::Pipe) {
                if (token_is_primary(token)) {
                    scanner->revert();
                    break;
                }
                else {
                    skip_to_next_schema_primary_token();
                    return nullptr;
                }
            }
            if (!scan_expected(GraphQlToken::G_Name)) {
                skip_to_next_schema_primary_token();
                return nullptr;
            }
            auto name = get_token_value();
            _union->members.emplace(name, create_syntax<Name>(SyntaxKind::S_Name, name));
        }
        symbols.emplace(token_value, create_symbol(&_union->name->identifier, _union, SymbolKind::SL_Union));
        unions.push_back(_union);
        return _union;
    }

    Syntax*
    GraphQlParser::parse_schema_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::SchemaKeyword:
                return parse_schema();
            case GraphQlToken::InputKeyword:
                return parse_input_object();
            case GraphQlToken::TypeKeyword:
                return parse_object();
            case GraphQlToken::InterfaceKeyword:
                return parse_interface();
            case GraphQlToken::UnionKeyword:
                return parse_union_after_union_keyword();
            default:
                add_diagnostic(D::Expected_type_definition);
                return nullptr;
        }
    }

    Schema*
    GraphQlParser::parse_schema()
    {
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            scanner->skip_block();
            return nullptr;
        }
        auto schema = create_syntax<Schema>(SyntaxKind::S_Schema);
        RootType added_types = RootType::None;
        RootType added_duplicate_types = RootType::None;
        while (true) {
            switch (take_next_token()) {
                case GraphQlToken::QueryKeyword:
                    if ((added_types & RootType::Query) == RootType::None) {
                        schema->query_key = create_syntax<Name>(SyntaxKind::S_Name, "query");
                        added_types = added_types | RootType::Query;
                        schema->query = parse_schema_field_after_colon();
                    }
                    else {
                        add_diagnostic(D::Duplicate_field_0, "query");
                        parse_schema_field_after_colon();
                        if ((added_duplicate_types & RootType::Query) == RootType::None) {
                            add_diagnostic(
                                get_location_from_syntax(schema->query_key),
                                D::Duplicate_field_0,
                                "query"
                            );
                            added_duplicate_types = added_duplicate_types | RootType::Query;
                        }
                    }
                    break;
                case GraphQlToken::MutationKeyword:
                    if ((added_types & RootType::Mutation) == RootType::None) {
                        schema->mutation_key = create_syntax<Name>(SyntaxKind::S_Name, "mutation");
                        added_types = added_types | RootType::Mutation;
                        schema->mutation = parse_schema_field_after_colon();
                    }
                    else {
                        add_diagnostic(D::Duplicate_field_0, "mutation");
                        parse_schema_field_after_colon();
                        if ((added_duplicate_types & RootType::Mutation) == RootType::None) {
                            add_diagnostic(
                                get_location_from_syntax(schema->mutation_key),
                                D::Duplicate_field_0,
                                "mutation"
                            );
                            added_duplicate_types = added_duplicate_types | RootType::Mutation;
                        }
                    }
                    break;
                case GraphQlToken::SubscriptionKeyword:
                    if ((added_types & RootType::Subscription) == RootType::None) {
                        schema->subscription_key = create_syntax<Name>(SyntaxKind::S_Name, "subscription");
                        added_types = added_types | RootType::Subscription;
                        schema->subscription = parse_schema_field_after_colon();
                    }
                    else {
                        add_diagnostic(D::Duplicate_field_0, "subscription");
                        parse_schema_field_after_colon();
                        if ((added_duplicate_types & RootType::Subscription) == RootType::None) {
                            add_diagnostic(
                                get_location_from_syntax(schema->subscription_key),
                                D::Duplicate_field_0,
                                "subscription"
                            );
                            added_duplicate_types = added_duplicate_types | RootType::Subscription;
                        }
                    }
                    break;
                case GraphQlToken::EndOfDocument:
                case GraphQlToken::CloseBrace:
                    goto outer;
                default:
                    add_diagnostic(D::A_schema_definition_can_only_contain_query_mutation_and_subscription_fields);
                    parse_schema_field_after_colon();
            }
        }
        outer:;
    }

    Name*
    GraphQlParser::parse_schema_field_after_colon()
    {
        if (!scan_expected(GraphQlToken::Colon)) {
            return nullptr;
        }
        if (!scan_expected(GraphQlToken::G_Name)) {
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        return create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
    }

    Name *
    GraphQlParser::parse_object_name(ObjectLike *object, SymbolKind kind)
    {
        if (!scan_expected(GraphQlToken::G_Name, D::Expected_type_name_instead_got_0, /*treat_keyword_as_name*/true)) {
            return nullptr;
        }
        auto token_value = get_token_value();
        auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
        const auto& result = symbols.find(token_value);
        if (result == symbols.end()) {
            symbols.emplace(token_value, create_symbol(&name->identifier, object, kind));
        }
        else {
            add_diagnostic(get_location_from_syntax(name), D::Duplicate_type_0, token_value);
            if (duplicate_symbols.count(token_value) == 0) {
                add_diagnostic(get_location_from_syntax(result->second->declaration->name), D::Duplicate_type_0, *result->second->name);
                duplicate_symbols.insert(token_value);
            }
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
            add_diagnostic(get_location_from_syntax(name), D::Duplicate_type_0, token_value);
            if (duplicate_symbols.count(token_value) == 0) {
                add_diagnostic(get_location_from_syntax(result->second->declaration->name), D::Duplicate_type_0, *result->second->name);
                duplicate_symbols.insert(token_value);
            }
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
            GraphQlToken field_token = take_next_token(/*treat_keyword_as_name*/ true);
            if (field_token == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_reached_the_end_of_document, "}");
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
                add_diagnostic(D::Expected_0_but_reached_the_end_of_document, "}");
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
            if (input_field_definition->type->is_non_null) {
                object->required_fields.push_back(name);
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
            GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true);
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
            const auto& name = get_token_value();
            input_value_definition->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (!scan_expected(GraphQlToken::Colon)) {
                return nullptr;
            }
            input_value_definition->type = parse_type_annotation(/*is_input*/ true);
            arguments_definition->input_value_definitions.push_back(input_value_definition);
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

    Syntax*
    GraphQlParser::parse_executable_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::QueryKeyword: {
                auto query = symbols.find("Query");
                if (query == symbols.end()) {

                    return nullptr;
                }
                current_object_types.push(dynamic_cast<Object*>(query->second->declaration));
                return parse_operation_definition(OperationType::Query);
            }
            case GraphQlToken::MutationKeyword: {
                auto query = symbols.find("Mutation");
                if (query == symbols.end()) {
                    // TODO: add diagnostic
                    return nullptr;
                }
                current_object_types.push(dynamic_cast<Object*>(query->second->declaration));
                return parse_operation_definition(OperationType::Mutation);
            }
            case GraphQlToken::SubscriptionKeyword: {
                auto query = symbols.find("Subscription");
                if (query == symbols.end()) {
                    // TODO: add diagnostic
                    return nullptr;
                }
                current_object_types.push(dynamic_cast<Object*>(query->second->declaration));
                return parse_operation_definition(OperationType::Subscription);
            }
            case GraphQlToken::OpenBrace: {
                auto query = symbols.find("Query");
                if (query == symbols.end()) {
                    return nullptr;
                }
                current_object_types.push(dynamic_cast<Object*>(query->second->declaration));
                auto operation_definition = create_syntax<OperationDefinition>(SyntaxKind::S_OperationDefinition);
                operation_definition->operation_type = OperationType::Query;
                return parse_operation_definition_body_after_open_brace(operation_definition);
            }
            case GraphQlToken::FragmentKeyword:
                return parse_fragment_definition_after_fragment_keyword();
            default:
                add_diagnostic(D::Expected_operation_definition_or_fragment_definition);
                return nullptr;
        }
    }

    FragmentDefinition*
    GraphQlParser::parse_fragment_definition_after_fragment_keyword()
    {
        auto fragment = create_syntax<FragmentDefinition>(SyntaxKind::S_FragmentDefinition);
        if (!scan_expected(GraphQlToken::G_Name)) {
            return nullptr;
        }
        auto token_value = get_token_value();
        fragment->name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
        const auto& fragments_it = fragments.find(token_value);
        if (fragments_it != fragments.end()) {
            add_diagnostic(D::Duplicate_fragment_0, token_value);
            if (duplicate_fragments.find(token_value) == duplicate_fragments.end()) {
                add_diagnostic(
                    get_location_from_syntax(fragments_it->second->name),
                    D::Duplicate_fragment_0,
                    fragments_it->second->name->identifier
                );
            }
        }
        else {
            fragments.emplace(token_value, fragment);
        }
        if (!scan_expected(GraphQlToken::OnKeyword)) {
            return nullptr;
        }
        if (!scan_expected(GraphQlToken::G_Name)) {
            return nullptr;
        }
        const auto& name = get_token_value();
        const auto& it = symbols.find(name);
        if (it == symbols.end()) {
            add_diagnostic(D::Type_0_is_not_defined, name);
            return nullptr;
        }
        const auto& object = static_cast<Object*>(it->second->declaration);
        if (object->kind != SyntaxKind::S_Object) {
            add_diagnostic(D::The_type_0_is_not_an_object, name);
        }
        current_object_types.push(object);
        fragment->type = create_syntax<Name>(SyntaxKind::S_Name, name);
        fragment->selection_set = parse_selection_set();
        if (fragment->selection_set == nullptr) {
            scanner->skip_block();
            return nullptr;
        }
        return fragment;
    }

    OperationDefinition* GraphQlParser::parse_operation_definition(const OperationType& operation_type)
    {
        auto operation_definition = create_syntax<OperationDefinition>(SyntaxKind::S_OperationDefinition);
        operation_definition->operation_type = operation_type;
        if (scan_optional(GraphQlToken::G_Name, /*treat_keyword_as_name*/true)) {
            operation_definition->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value().c_str());
            if (scan_optional(GraphQlToken::OpenParen)) {
                operation_definition->variable_definitions = parse_variable_definitions();
                scan_expected(GraphQlToken::CloseParen);
            }
        }
        return parse_operation_definition_body(operation_definition);
    }

    OperationDefinition*
    GraphQlParser::parse_operation_definition_body(OperationDefinition* operation_definition)
    {
        operation_definition->selection_set = parse_selection_set();
        if (operation_definition->selection_set == nullptr) {
            return nullptr;
        }
        return operation_definition;
    }

    OperationDefinition*
    GraphQlParser::parse_operation_definition_body_after_open_brace(OperationDefinition* operation_definition)
    {
        operation_definition->selection_set = parse_selection_set_after_open_brace();
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
            std::size_t start_position = current_position();
            GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true);
            switch (token) {
                case GraphQlToken::Ellipses: {
                    GraphQlToken token = take_next_token();
                    if (token == GraphQlToken::OnKeyword) {
                        if (!scan_expected(GraphQlToken::G_Name)) {
                            add_diagnostic(D::Expected_name_of_object);
                            scanner->skip_block();
                            return nullptr;
                        }
                        const auto& token_value = get_token_value();
                        const auto& name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
                        auto it = symbols.find(token_value);
                        if (it == symbols.end()) {
                            add_diagnostic(D::Type_0_is_not_defined, token_value);
                            scanner->skip_block();
                            break;
                        }
                        auto inline_fragment = create_syntax<InlineFragment>(SyntaxKind::S_InlineFragment);
                        inline_fragment->start = start_position;
                        const auto& object = static_cast<Object*>(it->second->declaration);
                        if (object->kind != SyntaxKind::S_Object) {
                            add_diagnostic(D::The_type_0_is_not_an_object, token_value);
                            scanner->skip_block();
                            break;
                        }
                        auto current_object_type = static_cast<Declaration*>(current_object_types.top());
                        check_fragment_assignment(name, object, current_object_type);
                        current_object_types.push(static_cast<ObjectLike*>(object));
                        inline_fragment->selection_set = parse_selection_set();
                        finish_syntax(inline_fragment);
                        selection_set->selections.push_back(inline_fragment);
                    }
                    else if (token == GraphQlToken::G_Name) {
                        auto name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
                        forward_fragment_references.emplace_back(name, current_object_types.top());
                    }
                    else {
                        add_diagnostic(D::Expected_fragment_spread_or_inline_fragment);
                        scanner->skip_block();
                    }
                    break;
                }

                case GraphQlToken::G_Name: {
                    auto token_value = get_token_value();
                    auto current_object_type = current_object_types.top();
                    auto& fields = current_object_type->fields;
                    auto field_definition_it = fields.find(token_value);
                    if (field_definition_it == fields.end()) {
                        add_diagnostic(D::Field_0_does_not_exist_on_type_1, token_value, current_object_type->name->identifier);
                        scanner->skip_to({
                            GraphQlToken::G_Name,
                            GraphQlToken::CloseBrace,
                            GraphQlToken::Ellipses,
                        });
                        continue;
                    }
                    auto field = create_syntax<Field>(SyntaxKind::S_Field);
                    auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
                    if (scan_optional(GraphQlToken::Colon)) {
                        field->alias = name;
                        field->name = parse_expected_name();
                    }
                    else {
                        field->name = name;
                    }
                    if (scan_optional(GraphQlToken::OpenParen)) {
                        current_field = field_definition_it->second;
                        field->arguments = parse_arguments();
                    }
                    if (scan_optional(GraphQlToken::OpenBrace)) {
                        if ((field_definition_it->second->type->type & TypeEnum::T_ObjectType) != TypeEnum::T_None) {
                            auto object = symbols.find(field_definition_it->second->type->name->identifier)->second->declaration;
                            current_object_types.push(static_cast<ObjectLike*>(object));
                        }
                        field->selection_set = parse_selection_set_after_open_brace();
                    }
                    selection_set->selections.push_back(field);
                    break;
                }

                case GraphQlToken::CloseBrace:
                    current_object_types.pop();
                    goto outer;

                case GraphQlToken::EndOfDocument: {
                    if (selection_set->selections.size() == 0) {
                        add_diagnostic(D::Expected_at_least_a_field_inline_fragment_or_fragment_spread);
                    }
                    else {
                        add_diagnostic(D::Expected_0_but_reached_the_end_of_document, "}");
                    }
                    return nullptr;
                }

                default:
                    add_diagnostic(D::Expected_field_inline_fragment_or_fragment_spread);
                    goto outer;
            }
        }
        outer:
        finish_syntax(selection_set);
        return selection_set;
    }

    void
    GraphQlParser::check_fragment_assignment(Name* name, Object* object, Declaration* current_object_type) {
        if (current_object_type->kind == SyntaxKind::S_Interface) {
            if (object->implementations == nullptr || object->implementations->implementations.count(current_object_type->name->identifier) == 0) {
                add_diagnostic(
                    get_location_from_syntax(name),
                    D::The_object_0_does_not_implement_the_interface_1,
                    object->name->identifier,
                    current_object_type->name->identifier
                );
            }
        }
        else if (current_object_type->kind == SyntaxKind::S_Object) {
            if (current_object_type->name->identifier != object->name->identifier) {
                add_diagnostic(
                    get_location_from_syntax(name),
                    D::The_object_0_does_not_match_the_object_1,
                    object->name->identifier,
                    current_object_type->name->identifier
                );
            }
        }
        else if (current_object_type->kind == SyntaxKind::S_Union) {
            const auto& members = static_cast<Union*>(current_object_type)->members;
            if (members.find(object->name->identifier) == members.end()) {
                add_diagnostic(
                    get_location_from_syntax(name),
                    D::The_type_0_is_not_member_of_the_union_1,
                    object->name->identifier,
                    current_object_type->name->identifier
                );
            }
        }
        else {
            add_diagnostic(
                get_location_from_syntax(name),
                D::Type_conditions_are_only_applicable_inside_object_interfaces_and_unions
            );
        }
    }

    Arguments* GraphQlParser::parse_arguments()
    {
        const auto& arguments = create_syntax<Arguments>(SyntaxKind::S_Arguments);
        auto input_value_definitions = current_field->arguments_definition->input_value_definitions;
        while (true) {
            auto token = take_next_token(/*treat_keyword_as_name*/true);
            if (token != GraphQlToken::G_Name) {
                break;
            }
            auto token_value = get_token_value();
            auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
            auto argument = create_syntax<Argument>(SyntaxKind::S_Argument);
            argument->name = name;
            auto result = std::find_if(input_value_definitions.begin(), input_value_definitions.end(), [&](InputValueDefinition* input_value_definition) {
                return input_value_definition->name->identifier == token_value;
            });
            bool is_defined = true;
            if (result == input_value_definitions.end()) {
                add_diagnostic(D::Unknown_argument_0, token_value);
                is_defined = false;
            }
            auto duplicate_result = parsed_arguments.find(token_value);
            if (duplicate_result != parsed_arguments.end()) {
                duplicate_arguments.emplace(token_value);
            }
            else {
                parsed_arguments.emplace(token_value);
            }
            if (!scan_expected(GraphQlToken::Colon)) {
                scanner->skip_to({ GraphQlToken::CloseBrace });
                return nullptr;
            }
            if (is_defined) {
                argument->value = parse_value((*result)->type);
            }
            else {
                scanner->skip_to({
                    GraphQlToken::G_Name,
                    GraphQlToken::CloseParen,
                });
            }
            arguments->arguments.push_back(argument);
        }
        if (duplicate_arguments.size() > 0) {
            for (const auto& duplicate_argument : duplicate_arguments) {
                for (const auto& argument : arguments->arguments) {
                    if (argument->name->identifier == duplicate_argument) {
                        add_diagnostic(
                            get_location_from_syntax(argument->name),
                            D::Duplicate_argument_0,
                            duplicate_argument
                        );
                    }
                }
            }
            duplicate_arguments.clear();
        }
        parsed_arguments.clear();
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
                variable_definition->default_value = parse_value(variable_definition->type);
            }
            if (!scan_optional(GraphQlToken::Comma)) {
                break;
            }
            variable_definitions->variable_definitions.push_back(variable_definition);
        }
        return finish_syntax(variable_definitions);
    }

    Value*
    GraphQlParser::parse_value(Type* type)
    {
        GraphQlToken token = take_next_token();
        switch (token) {
            case GraphQlToken::NullKeyword:
                if (type->is_non_null) {
                    add_diagnostic(
                        D::The_value_0_is_not_assignable_to_type_1,
                        "null",
                        get_type_name(type)
                    );
                }
                return create_syntax<NullValue>(SyntaxKind::S_NullValue);
            case GraphQlToken::StringLiteral: {
                if (type->type != TypeEnum::T_String) {
                    add_diagnostic(
                        D::Type_0_is_not_assignable_to_type_1,
                        "String",
                        get_type_name(type)
                    );
                }
                auto value = create_syntax<StringValue>(SyntaxKind::S_StringValue);
                value->value = get_token_value();
                return value;
            }
            case GraphQlToken::IntegerLiteral: {
                if (type->type != TypeEnum::T_Int) {
                    add_diagnostic(
                        D::Type_0_is_not_assignable_to_type_1,
                        "Int",
                        get_type_name(type)
                    );
                }
                auto value = create_syntax<IntValue>(SyntaxKind::S_IntValue);
                value->value = std::atoi(get_token_value().c_str());
                return value;
            }
            case GraphQlToken::FloatLiteral: {
                if (type->type != TypeEnum::T_Float) {
                    add_diagnostic(
                        D::Type_0_is_not_assignable_to_type_1,
                        "Float",
                        get_type_name(type)
                    );
                }
                auto value = create_syntax<FloatValue>(SyntaxKind::S_FloatValue);
                value->value = std::atof(get_token_value().c_str());
                return value;
            }
            case GraphQlToken::BooleanLiteral: {
                auto value = create_syntax<BooleanValue>(SyntaxKind::S_BooleanValue);
                value->value = get_token_value().at(0) == 't';
                return value;
            }
//            case GraphQlToken::Name:
//                return Value {
//                    ValueType::Enum,
//                    create_syntax<EnumLiteral>(SyntaxKind::EnumLiteral, get_token_value().c_str()),
//                };
//            case GraphQlToken::Dollar:
//                ArgumentLiteral argument_literal = create_syntax<ArgumentLiteral>(SyntaxKind::ArgumentLiteral);
//                argument_literal.name = parse_name();
//                finish_syntax(argument_literal);
//                return std::make_pair(argument_literal, ValueKind::Argument);
            case GraphQlToken::OpenBrace: {
                auto object_value = create_syntax<ObjectValue>(SyntaxKind::S_ObjectValue);
                auto symbols_it = symbols.find(type->name->identifier);
                if (symbols_it->second->kind != SymbolKind::SL_InputObject) {
                    add_diagnostic(D::The_type_0_is_not_an_object, type->name->identifier);
                    return nullptr;
                }
                const auto& input_object = static_cast<InputObject*>(symbols_it->second->declaration);
                std::vector<Glib::ustring> parsed_fields;
                while (true) {
                    GraphQlToken token = take_next_token();
                    if (token == GraphQlToken::CloseBrace) {
                        break;
                    }
                    if (token == GraphQlToken::EndOfDocument) {
                        break;
                    }
                    if (token != GraphQlToken::G_Name) {
                        add_diagnostic(D::Expected_object_field_name);
                        auto t = scanner->skip_to({ GraphQlToken::CloseBrace, GraphQlToken::G_Name });
                        if (t == GraphQlToken::G_Name) {
                            continue;
                        }
                        break;
                    }
                    const auto& name = get_token_value();
                    const auto& field = input_object->fields.find(name);
                    if (field == input_object->fields.end()) {
                        add_diagnostic(
                            D::Field_0_does_not_exist_on_type_1,
                            name,
                            type->name->identifier
                        );
                        skip_to({
                            GraphQlToken::G_Name,
                            GraphQlToken::CloseBrace,
                        });
                        continue;
                    }
                    if (std::find(parsed_fields.begin(), parsed_fields.end(), name) != parsed_fields.end()) {
                        duplicate_fields.emplace(name);
                    }
                    parsed_fields.push_back(name);
                    auto object_field = create_syntax<ObjectField>(SyntaxKind::S_ObjectField);
                    object_field->name = create_syntax<Name>(SyntaxKind::S_Name, name);
                    if (!scan_expected(GraphQlToken::Colon)) {
                        return nullptr;
                    }
                    object_field->value = parse_value(static_cast<Type*>(field->second));
                    object_value->object_fields.push_back(object_field);
                }
                finish_syntax(object_value);
                if (type->type != TypeEnum::T_Object) {
                    add_diagnostic(
                        get_location_from_syntax(object_value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        scanner->get_text_from_syntax(object_value),
                        type->name->identifier
                    );
                }

                if (duplicate_fields.size() > 0) {
                    for (const auto& duplicate_field : duplicate_fields) {
                        for (const auto& object_field : object_value->object_fields) {
                            if (object_field->name->identifier == duplicate_field) {
                                add_diagnostic(
                                    get_location_from_syntax(object_field->name),
                                    D::Duplicate_field_0,
                                    duplicate_field
                                );
                            }
                        }
                    }
                    duplicate_arguments.clear();
                }

                for (const auto& required_field : input_object->required_fields) {
                    if (std::find(parsed_fields.begin(), parsed_fields.end(), required_field) == parsed_fields.end()) {
                        add_diagnostic(
                            get_location_from_syntax(object_value),
                            D::Missing_required_field_0,
                            required_field
                        );
                    }
                }
                return object_value;
            }
            default:
                return nullptr;
        }
    }

    Location
    GraphQlParser::get_token_location()
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

    inline
    std::size_t
    GraphQlParser::current_position()
    {
        return scanner->position;
    }

    inline
    bool
    GraphQlParser::scan_optional(const GraphQlToken &token)
    {
        return scanner->try_scan(token) == token;
    }

    inline
    bool
    GraphQlParser::scan_optional(const GraphQlToken &token, bool treat_keyword_as_name)
    {
        return scanner->try_scan(token, treat_keyword_as_name) == token;
    }

    inline
    bool
    GraphQlParser::scan_expected(const GraphQlToken& token)
    {
        return scan_expected(token, false);
    }

    inline
    bool
    GraphQlParser::scan_expected(const GraphQlToken& token, bool treat_keyword_as_name)
    {
        GraphQlToken result = scanner->scan_expected(token, treat_keyword_as_name);
        if (result != token) {
            if (result == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_reached_the_end_of_document, graphQlTokenToString.at(token));
            }
            else {
                add_diagnostic(D::Expected_0_but_got_1, graphQlTokenToString.at(token), get_token_value());
            }
            return false;
        }
        return true;
    }

    inline
    bool
    GraphQlParser::scan_expected(const GraphQlToken& token, DiagnosticMessageTemplate& _template)
    {
        GraphQlToken result = scanner->scan_expected(token);
        if (result != token) {
            add_diagnostic(_template, get_token_value());
            return false;
        }
        return true;
    }

    inline
    bool
    GraphQlParser::scan_expected(const GraphQlToken& token, DiagnosticMessageTemplate& _template, bool treat_keyword_as_name)
    {
        GraphQlToken result = scanner->scan_expected(token, treat_keyword_as_name);
        if (result != token) {
            add_diagnostic(_template, get_token_value());
            return false;
        }
        return true;
    }

    template<typename T, typename ... Args>
    T*
    GraphQlParser::create_syntax(SyntaxKind kind, Args ... args) {
        return new (memory_pool, ticket) T (kind, scanner->start_position, scanner->position, args...);
    }

    Symbol*
    GraphQlParser::create_symbol(Glib::ustring* name, Declaration* declaration, SymbolKind kind)
    {
        return new (memory_pool, ticket) Symbol(name, declaration, kind);
    }

    template<typename S>
    inline
    S*
    GraphQlParser::finish_syntax(S* syntax)
    {
        syntax->end = scanner->start_position + scanner->length();
        return syntax;
    }

    inline
    GraphQlToken
    GraphQlParser::take_next_token()
    {
        return scanner->take_next_token(false);
    }

    inline
    GraphQlToken
    GraphQlParser::take_next_token(bool treat_keyword_as_name)
    {
        return scanner->take_next_token(treat_keyword_as_name);
    }

    inline
    Glib::ustring
    GraphQlParser::get_token_value() const
    {
        return scanner->get_value();
    }

    inline
    Glib::ustring
    GraphQlParser::get_string_value() {
        return scanner->get_string_value();
    }


    Glib::ustring
    GraphQlParser::get_type_name(Type *type)
    {
        if (type->name != nullptr) {
            return type->name->identifier;
        }
        else {
            Glib::ustring display_type = "";
            if (type->is_list_type) {
                display_type += "[";
            }
            switch (type->type) {
                case TypeEnum::T_Boolean:
                    display_type += "Boolean";
                    break;
                case TypeEnum::T_Int:
                    display_type += "Int";
                    break;
                case TypeEnum::T_Float:
                    display_type += "Float";
                    break;
                case TypeEnum::T_String:
                    display_type += "String";
                    break;
                case TypeEnum::T_ID:
                    display_type += "ID";
                    break;
                default:
                    throw std::logic_error("Should not reach here.");
            }
            if (type->is_non_null) {
                display_type += "!";
            }
            if (type->is_list_type) {
                display_type += "]";
            }
            if (type->is_non_null_list) {
                display_type += "!";
            }
            return display_type;
        }
    }

    void
    GraphQlParser::skip_to_next_schema_primary_token()
    {
        scanner->skip_to({
            GraphQlToken::TypeKeyword,
            GraphQlToken::InputKeyword,
            GraphQlToken::UnionKeyword,
            GraphQlToken::InterfaceKeyword,
        });
    }

    bool
    GraphQlParser::token_is_primary(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::TypeKeyword:
            case GraphQlToken::InputKeyword:
            case GraphQlToken::UnionKeyword:
            case GraphQlToken::InterfaceKeyword:
            case GraphQlToken::EndOfDocument:
                return true;
            default:
                return false;
        }
    }

    inline
    GraphQlToken
    GraphQlParser::skip_to(const std::vector<GraphQlToken>& tokens)
    {
        return scanner->skip_to(tokens);
    }
}
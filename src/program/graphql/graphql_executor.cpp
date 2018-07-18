#include "graphql_executor.h"
//#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "graphql_diagnostics.h"
#include "lib/text_writer.h"
#include <memory>
#include <exception>

namespace flashpoint::program::graphql {

    GraphQlParser::GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket):
        memory_pool(memory_pool),
        ticket(ticket)
    { }

    SchemaDocument* GraphQlParser::add_schema(const Glib::ustring *schema_source)
    {
        scanner = new GraphQlScanner(schema_source);
        auto schema_document = create_syntax<SchemaDocument>(SyntaxKind::S_QueryDocument);
        schema_document->source = schema_source;
        GraphQlToken token = take_next_token();
        bool has_schema_query_root_operation = false;
        Schema* schema = nullptr;
        while (token != GraphQlToken::EndOfDocument) {
            if (token == GraphQlToken::G_StringValue) {
                has_description = true;
                current_description = get_string_value();
                token = take_next_token();
                continue;
            }
            auto type_definition = parse_schema_primary_token(token);
            has_description = false;
            if (type_definition != nullptr) {
                schema_document->definitions.push_back(type_definition);
                if (type_definition->kind == SyntaxKind::S_Schema) {
                    schema = static_cast<Schema*>(type_definition);
                }
            }
            else {
                scanner->skip_block();
            }
            token = take_next_token();
        }
        if (schema == nullptr) {
            schema = create_syntax<Schema>(SyntaxKind::S_Schema);
        }
        check_schema_references(schema);
        check_forward_references();
        check_directive_references();
        check_object_implementations();
        check_union_members();
        if (!diagnostics.empty()) {
            schema_document->diagnostics = diagnostics;
        }
        // TODO: Make mutliple schema resolvement
        query = schema->query_object;
        mutation = schema->mutation_object;
        subscription = schema->subscription_object;
        finish_syntax(schema_document);
        return schema_document;
    }

    void
    GraphQlParser::check_schema_references(Schema* schema)
    {
        if (schema->query != nullptr) {
            auto result = symbols.find(schema->query->identifier);
            if (result != symbols.end()) {
                schema->query_object = static_cast<Object*>(result->second->declaration);
            }
            else {
                add_diagnostic(D::Type_0_is_not_defined, schema->query->identifier);
            }
        }
        else {
            auto result = symbols.find("Query");
            if (result != symbols.end()) {
                schema->query_object = static_cast<Object*>(result->second->declaration);
            }
            else {
                add_diagnostic({ 0, 0, 0, true }, D::No_query_root_operation_type_defined);
            }
        }

        if (schema->mutation != nullptr) {
            auto result = symbols.find(schema->mutation->identifier);
            if (result != symbols.end()) {
                schema->mutation_object = static_cast<Object*>(result->second->declaration);
            }
            else {
                add_diagnostic(D::Type_0_is_not_defined, schema->mutation->identifier);
            }
        }
        else {
            auto result = symbols.find("Mutation");
            if (result != symbols.end()) {
                schema->mutation_object = static_cast<Object*>(result->second->declaration);
            }
        }

        if (schema->subscription != nullptr) {
            auto result = symbols.find(schema->subscription->identifier);
            if (result != symbols.end()) {
                schema->subscription_object = static_cast<Object*>(result->second->declaration);
            }
            else {
                add_diagnostic(D::Type_0_is_not_defined, schema->subscription->identifier);
            }
        }
        else {
            auto result = symbols.find("Subscription");
            if (result != symbols.end()) {
                schema->subscription_object = static_cast<Object*>(result->second->declaration);
            }
        }
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
                token = take_next_token();
                continue;
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
        check_directive_references();
        if (!diagnostics.empty()) {
            executable_definition->diagnostics = diagnostics;
        }
        return executable_definition;
    }

    void
    GraphQlParser::check_forward_references()
    {
        for (const auto& type : forward_type_references) {
            auto it = symbols.find(type->name->identifier);
            if (it == symbols.end()) {
                add_diagnostic(get_location_from_syntax(type->name), D::Type_0_is_not_defined, type->name->identifier);
            }
            else {
                if ((it->second->kind & SymbolKind::SL_Enum) > SymbolKind::SL_None) {
                    type->type = TypeEnum::T_Enum;
                    if (type->parent_directive_definition != nullptr) {
                        check_recursive_directives(type->parent_directive_definition->name->identifier, type, type);
                    }
                }
                else {
                    if ((it->second->kind & SymbolKind::Input) > SymbolKind::SL_None) {
                        if (!type->in_input_location) {
                            add_diagnostic(get_location_from_syntax(type->name), D::Cannot_add_an_input_type_to_an_output_location, type->name->identifier);
                        }
                        if (type->parent_directive_definition != nullptr) {
                            check_recursive_directives(type->parent_directive_definition->name->identifier, type, type);
                        }
                    }
                    if ((it->second->kind & SymbolKind::Output) > SymbolKind::SL_None && type->in_input_location) {
                        add_diagnostic(get_location_from_syntax(type->name), D::Cannot_add_an_output_type_to_an_input_location, type->name->identifier);
                    }
                }
            }
        }
    }

    void
    GraphQlParser::check_directive_references()
    {
        for (const auto& directive : directives) {
            auto definition = directive_definitions.find(directive->name->identifier);
            if (definition == directive_definitions.end()) {
                add_diagnostic(get_location_from_syntax(directive), D::Undefined_directive_0, directive->name->identifier);
                continue;
            }
            if (directive->parent_directive_definition != nullptr &&
                directive->parent_directive_definition->name->identifier == directive->name->identifier)
            {
                add_diagnostic(get_location_from_syntax(directive), D::Directive_cannot_reference_itself);
            }
            auto location_it = definition->second->locations.find(directive->location);
            if (location_it == definition->second->locations.end()) {
                add_diagnostic(
                    get_location_from_syntax(directive),
                    D::The_directive_0_does_not_support_the_location_1,
                    "@" + directive->name->identifier,
                    directiveLocationToString.at(directive->location)
                );
            }
            std::set<Glib::ustring> checked_arguments;
            for (const auto& argument_definition_it : definition->second->arguments) {
                const auto& argument_definition = argument_definition_it.second;
                const auto& argument = argument_definition->name->identifier;
                const auto& directive_argument_it = directive->arguments.find(argument);
                if (directive_argument_it == directive->arguments.end()) {
                    if (argument_definition->type->is_non_null || argument_definition->type->is_non_null_list) {
                        add_diagnostic(get_location_from_syntax(directive), D::Missing_required_argument_0, argument);
                    }
                    continue;
                }
                checked_arguments.emplace(argument);
                const auto& directive_argument = directive_argument_it->second;
                check_value(directive_argument->value, argument_definition_it.second->type);
            }
            if (checked_arguments.size() < directive->arguments.size()) {
                for (const auto& argument_it : directive->arguments) {
                    if (checked_arguments.find(argument_it.second->name->identifier) != checked_arguments.end()) {
                        add_diagnostic(D::Unknown_argument_0);
                    }
                }
            }
        }
    }

    void
    GraphQlParser::check_recursive_directives(const Glib::ustring& parent_directive_definition, Type* location, Type* type)
    {
        if ((type->kind & SyntaxKind::S_InputTypeWithDirectives) == SyntaxKind::S_None) {
            return;
        }
        auto it = symbols.find(type->name->identifier);
        auto input_type = static_cast<InputType*>(it->second->declaration);
        auto result = input_type->directives.find(parent_directive_definition);
        if (result != input_type->directives.end()) {
            add_diagnostic(
                get_location_from_syntax(location),
                D::Cannot_annotate_with_a_type_that_recursively_references_the_directive_0,
                "@" + parent_directive_definition
            );
        }
        switch (input_type->kind) {
            case SyntaxKind::S_InputObject: {
                auto input_object = static_cast<InputObject*>(input_type);
                for (const auto& f : *input_object->fields) {
                    auto field_directive_it = f.second->directives.find(parent_directive_definition);
                    if (field_directive_it != f.second->directives.end()) {
                        add_diagnostic(
                            get_location_from_syntax(location),
                            D::Cannot_annotate_with_a_type_that_recursively_references_the_directive_0,
                            "@" + parent_directive_definition
                        );
                    }
                    if (f.second->type->type == TypeEnum::T_Object) {
                        check_recursive_directives(parent_directive_definition, location, f.second->type);
                    }
                }
                break;
            }
            case SyntaxKind::S_EnumTypeDefinition: {
                auto enum_type_definition = static_cast<EnumTypeDefinition*>(input_type);
                for (const auto& v : enum_type_definition->values) {
                    auto field_directive_it = v.second->directives.find(parent_directive_definition);
                    if (field_directive_it != v.second->directives.end()) {
                        add_diagnostic(
                            get_location_from_syntax(location),
                            D::Cannot_annotate_with_a_type_that_recursively_references_the_directive_0,
                            "@" + parent_directive_definition
                        );
                    }
                }
                break;
            }
            default:;
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
                    if (object->fields != nullptr) {
                        for (const auto& interfaceFieldIt : *interfaceIt->second->fields) {
                            const auto& objectFieldIt = object->fields->find(interfaceFieldIt.second->name->identifier);
                            if (objectFieldIt == object->fields->end()) {
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
        if (has_description) {
            object->description = current_description;
        }
        if (object->name == nullptr) {
            return nullptr;
        }
        object->directives = parse_directives(DirectiveLocation::OBJECT, nullptr);
        if (scan_optional(GraphQlToken::ImplementsKeyword)) {
            object->implementations = parse_implementations();
            if (object->implementations == nullptr) {
                return nullptr;
            }
            object->fields = parse_fields_definition_after_open_brace(object);
            objects_with_implementations.push_back(object);
        }
        else {
            object->fields = parse_fields_definition(object);
        }
        if (object->fields == nullptr) {
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
        if (has_description) {
            input_object->description = current_description;
        }
        if (input_object->name == nullptr) {
            return nullptr;
        }
        input_object->directives = parse_directives(DirectiveLocation::INPUT_OBJECT, nullptr);
        input_object->fields = parse_input_fields_definition(input_object);
        if (input_object->fields == nullptr) {
            return nullptr;
        }
        return input_object;
    }

    Interface*
    GraphQlParser::parse_interface_definition()
    {
        auto interface = create_syntax<Interface>(SyntaxKind::S_Interface);
        interface->name = parse_object_name(interface, SymbolKind::SL_Interface);
        if (has_description) {
            interface->description = current_description;
        }
        if (interface->name == nullptr) {
            return nullptr;
        }
        interface->directives = parse_directives(DirectiveLocation::INTERFACE, nullptr);
        interface->fields = parse_fields_definition(interface);
        interfaces.emplace(interface->name->identifier, interface);
        return interface;
    }

    UnionTypeDefinition*
    GraphQlParser::parse_union_after_union_keyword()
    {
        auto union_type_definition = create_syntax<UnionTypeDefinition>(SyntaxKind::S_Union);
        if (!scan_expected(GraphQlToken::G_Name)) {
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        auto token_value = get_token_value();
        union_type_definition->name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
        union_type_definition->directives = parse_directives(DirectiveLocation::UNION, nullptr);
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
        union_type_definition->members.emplace(member_name ,create_syntax<Name>(SyntaxKind::S_Name, get_token_value()));
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
            union_type_definition->members.emplace(name, create_syntax<Name>(SyntaxKind::S_Name, name));
        }
        symbols.emplace(token_value, create_symbol(&union_type_definition->name->identifier, union_type_definition, SymbolKind::SL_Union));
        unions.push_back(union_type_definition);
        return union_type_definition;
    }

    DirectiveDefinition*
    GraphQlParser::parse_directive_definition_after_directive_keyword()
    {
        if (!scan_expected(GraphQlToken::At)) {
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/false);
        if (token != GraphQlToken::G_Name) {
            add_diagnostic(D::Expected_name_but_instead_got_0, graphQlTokenToString.at(token));
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        auto definition = create_syntax<DirectiveDefinition>(SyntaxKind::S_DirectiveDefinition);
        if (has_description) {
            definition->description = current_description;
        }
        definition->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
        if (scan_optional(GraphQlToken::OpenParen)) {
            definition->arguments = parse_arguments_definition_after_open_paren(definition);
        }
        if (!scan_expected(GraphQlToken::OnKeyword)) {
            skip_to({ GraphQlToken::CloseBrace });
            return nullptr;
        }
        scan_optional(GraphQlToken::Pipe);
        auto location = scanner->scan_directive_location();
        if (location == DirectiveLocation::Unknown) {
            add_diagnostic(D::Expected_directive_location);
            skip_to_next_schema_primary_token();
            return nullptr;
        }
        definition->locations.emplace(location);
        while (true) {
            scanner->save();
            GraphQlToken token = take_next_token();
            if (token_is_primary(token)) {
                scanner->revert();
                break;
            }
            if (token != GraphQlToken::Pipe) {
                add_diagnostic(D::Expected_0_but_got_1, "|", get_token_value());
                skip_to_next_schema_primary_token();
                return nullptr;
            }
            auto l = scanner->scan_directive_location();
            if (l == DirectiveLocation::Unknown) {
                add_diagnostic(D::Expected_directive_location);
                skip_to_next_schema_primary_token();
                return nullptr;
            }
            if (l == DirectiveLocation::EndOfDocument) {
                add_diagnostic(Location { 0, 0, 0, true }, D::Expected_directive_location_but_instead_reached_the_end_of_document);
                return nullptr;
            }
            definition->locations.emplace(l);
        }
        directive_definitions.emplace(definition->name->identifier, definition);
        return definition;
    }

    EnumTypeDefinition*
    GraphQlParser::parse_enum()
    {
        if (!scan_expected(GraphQlToken::G_Name)) {
            return nullptr;
        }
        auto enum_type_definition = create_syntax<EnumTypeDefinition>(SyntaxKind::S_EnumTypeDefinition);
        const auto& enum_name = get_token_value();
        enum_type_definition->name = create_syntax<Name>(SyntaxKind::S_Name, enum_name);
        const auto& result = symbols.find(enum_name);
        if (result == symbols.end()) {
            symbols.emplace(enum_name, create_symbol(&enum_type_definition->name->identifier, enum_type_definition, SymbolKind::SL_Enum));
        }
        else {
            add_diagnostic(get_location_from_syntax(enum_type_definition->name), D::Duplicate_type_0, enum_name);
            if (duplicate_symbols.count(enum_name) == 0) {
                add_diagnostic(get_location_from_syntax(result->second->declaration->name), D::Duplicate_type_0, *result->second->name);
                duplicate_symbols.insert(enum_name);
            }
        }
        enum_type_definition->directives = parse_directives(DirectiveLocation::ENUM, nullptr);
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            return nullptr;
        }
        GraphQlToken token;
        bool has_at_least_one_field = false;
        while(true) {
            Glib::ustring current_description = "";
            token = take_next_token();
            if (token == GraphQlToken::CloseBrace && has_at_least_one_field) {
                break;
            }
            if (token == GraphQlToken::G_StringValue) {
                current_description = get_string_value();
                continue;
            }
            if (token == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_instead_reached_the_end_of_document, "enum value");
                return nullptr;
            }
            if (!is_valid_enum_value(token)) {
                add_diagnostic(D::Expected_enum_value_but_instead_got_0, get_token_value());
                skip_to({ GraphQlToken::CloseBrace });
                break;
            }
            const auto& token_value = get_token_value();
            const auto& enum_value = create_syntax<EnumValueDefinition>(SyntaxKind::S_Name, token_value);
            enum_value->directives = parse_directives(DirectiveLocation::ENUM_VALUE, nullptr);
            enum_value->description = current_description;
            enum_type_definition->values.emplace(token_value, enum_value);
            has_at_least_one_field = true;
        }
        return enum_type_definition;
    }

    inline
    bool
    GraphQlParser::is_valid_enum_value(GraphQlToken token)
    {
        if (token == GraphQlToken::G_Name) {
            return true;
        }
        switch (token) {
            case GraphQlToken::TrueKeyword:
            case GraphQlToken::FalseKeyword:
            case GraphQlToken::NullKeyword:
            case GraphQlToken::Unknown:
                return false;
            default:;
        }
        if (token > GraphQlToken::StartKeyword && token < GraphQlToken::EndKeyword) {
            return true;
        }
        return false;
    }

    Syntax*
    GraphQlParser::parse_schema_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::DirectiveKeyword:
                return parse_directive_definition_after_directive_keyword();
            case GraphQlToken::EnumKeyword:
                return parse_enum();
            case GraphQlToken::InterfaceKeyword:
                return parse_interface_definition();
            case GraphQlToken::InputKeyword:
                return parse_input_object();
            case GraphQlToken::SchemaKeyword:
                return parse_schema();
            case GraphQlToken::TypeKeyword:
                return parse_object();
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
        auto directives = parse_directives(DirectiveLocation::SCHEMA, nullptr);
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            scanner->skip_block();
            return nullptr;
        }
        auto schema = create_syntax<Schema>(SyntaxKind::S_Schema);
        schema->directives = directives;
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
        outer:
        return schema;
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
        auto name = get_token_value();
        return create_syntax<Name>(SyntaxKind::S_Name, name);
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

    std::map<Glib::ustring, FieldDefinition*>*
    GraphQlParser::parse_fields_definition(ObjectLike* object)
    {
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            return nullptr;
        }
        return parse_fields_definition_after_open_brace(object);
    }

    std::map<Glib::ustring, FieldDefinition*>*
    GraphQlParser::parse_fields_definition_after_open_brace(ObjectLike *object)
    {
        auto fields = new std::map<Glib::ustring, FieldDefinition*>();
        bool has_at_least_one_field = false;
        std::set<Glib::ustring> field_names;
        Glib::ustring current_description;
        while (true) {
            GraphQlToken field_token = take_next_token(/*treat_keyword_as_name*/ true, /*skip_white_space*/true);
            if (field_token == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_instead_reached_the_end_of_document, "}");
                return nullptr;
            }
            if (field_token == GraphQlToken::CloseBrace && has_at_least_one_field) {
                break;
            }
            if (field_token == GraphQlToken::G_StringValue) {
                current_description = get_string_value();
                continue;
            }
            if (field_token != GraphQlToken::G_Name) {
                if (field_token == GraphQlToken::InvalidString) {
                    take_errors_from_scanner();
                    continue;
                }
                else {
                    add_diagnostic(D::Expected_field_definition);
                }
                return nullptr;
            }
            auto field_definition = create_syntax<FieldDefinition>(SyntaxKind::S_FieldDefinition);
            field_definition->description = current_description;
            auto name = get_token_value();
            if (field_names.find(name) != field_names.end()) {
                add_diagnostic(D::Duplicate_field_0, name);
            }
            field_names.emplace(name);
            field_definition->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (scan_optional(GraphQlToken::OpenParen)) {
                field_definition->arguments = parse_arguments_definition_after_open_paren(nullptr);
            }
            if (!scan_expected(GraphQlToken::Colon)) {
                return nullptr;
            }
            field_definition->type = parse_type_annotation(/*is_input*/ false, nullptr);
            if (field_definition->type == nullptr) {
                return nullptr;
            }
            field_definition->directives = parse_directives(DirectiveLocation::FIELD_DEFINITION, nullptr);
            fields->emplace(name, field_definition);
            has_at_least_one_field = true;
            current_description = "";
        }
        return fields;
    }

    std::map<Glib::ustring, InputFieldDefinition*>*
    GraphQlParser::parse_input_fields_definition(InputObject* object)
    {
        auto fields = new std::map<Glib::ustring, InputFieldDefinition*>();
        if (!scan_expected(GraphQlToken::OpenBrace)) {
            return nullptr;
        }
        bool has_at_least_one_field = false;
        Glib::ustring current_description;
        while (true) {
            auto input_field_definition = create_syntax<InputFieldDefinition>(SyntaxKind::S_InputFieldDefinition);
            GraphQlToken field_token = take_next_token(/*treat_keyword_as_name*/true);
            if (field_token == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_instead_reached_the_end_of_document, "}");
                return nullptr;
            }
            if (field_token == GraphQlToken::CloseBrace && has_at_least_one_field) {
                break;
            }
            if (field_token == GraphQlToken::G_StringValue) {
                current_description = get_string_value();
                continue;
            }
            if (field_token != GraphQlToken::G_Name) {
                if (field_token == GraphQlToken::InvalidString) {
                    take_errors_from_scanner();
                    continue;
                }
                else {
                    add_diagnostic(D::Expected_field_definition);
                }
                return nullptr;
            }
            auto name = get_token_value();
            input_field_definition->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (!scan_expected(GraphQlToken::Colon)) {
                return nullptr;
            }
            input_field_definition->type = parse_type_annotation(/*is_input*/ true, nullptr);
            if (input_field_definition->type == nullptr) {
                return nullptr;
            }
            if (input_field_definition->type->is_non_null || input_field_definition->type->is_non_null_list) {
                object->required_fields.push_back(name);
            }
            input_field_definition->directives = parse_directives(DirectiveLocation::INPUT_FIELD_DEFINITION, nullptr);
            fields->emplace(name, input_field_definition);
            has_at_least_one_field = true;
            current_description = "";
        }
        return fields;
    }

    std::map<Glib::ustring, InputValueDefinition*>
    GraphQlParser::parse_arguments_definition_after_open_paren(DirectiveDefinition* parent_directive_definition)
    {
        std::map<Glib::ustring, InputValueDefinition*> arguments = {};
        do {
            GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
            if (token == GraphQlToken::G_StringValue) {
                current_description = get_string_value();
                has_description = true;
                continue;
            }
            auto input_value_definition = create_syntax<InputValueDefinition>(SyntaxKind::S_InputValueDefinition);
            if (has_description) {
                input_value_definition->description = current_description;
                has_description = false;
            }
            if (token == GraphQlToken::CloseParen) {
                break;
            }
            if (token == GraphQlToken::EndOfDocument) {
                return arguments;
            }
            if (token != GraphQlToken::G_Name) {
                add_diagnostic(D::Expected_name_of_argument);
                return arguments;
            }
            const auto& name = get_token_value();
            input_value_definition->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (!scan_expected(GraphQlToken::Colon)) {
                return arguments;
            }
            input_value_definition->type = parse_type_annotation(/*is_input*/ true, parent_directive_definition);
            input_value_definition->directives = parse_directives(DirectiveLocation::ARGUMENT_DEFINITION, parent_directive_definition);
            if (arguments.find(name) == arguments.end()) {
                arguments.emplace(name, input_value_definition);
            }
            else {
                add_diagnostic(D::Duplicate_argument_0, name);
            }
        }
        while (true);
        return arguments;
    }

    Type*
    GraphQlParser::parse_type_annotation(bool in_input_location, DirectiveDefinition* parent_directive_definition)
    {
        GraphQlToken token = take_next_token();
        auto type = create_syntax<Type>(SyntaxKind::S_Type);
        type->parent_directive_definition = parent_directive_definition;
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
                type->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
                forward_type_references.push_back(type);
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
                current_object_types.push(query);
                return parse_operation_definition(OperationType::Query);
            }
            case GraphQlToken::MutationKeyword: {
                if (mutation == nullptr) {
                    add_diagnostic(D::Mutations_are_not_supported);
                    skip_to_next_query_primary_token();
                    return nullptr;
                }
                current_object_types.push(mutation);
                return parse_operation_definition(OperationType::Mutation);
            }
            case GraphQlToken::SubscriptionKeyword: {
                if (subscription == nullptr) {
                    add_diagnostic(D::Subscriptions_are_not_supported);
                    skip_to_next_query_primary_token();
                    return nullptr;
                }
                current_object_types.push(subscription);
                return parse_operation_definition(OperationType::Subscription);
            }
            case GraphQlToken::OpenBrace: {
                current_object_types.push(query);
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
        fragment->directives = parse_directives(DirectiveLocation::FRAGMENT_DEFINITION, nullptr);
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
        DirectiveLocation location;
        switch (operation_type) {
            case OperationType::Query:
                location = DirectiveLocation::QUERY;
                break;
            case OperationType::Mutation:
                location = DirectiveLocation::MUTATION;
                break;
            case OperationType::Subscription:
                location = DirectiveLocation::SUBSCRIPTION;
                break;
        }
        operation_definition->directives = parse_directives(location, nullptr);
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
            GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
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
                        inline_fragment->directives = parse_directives(DirectiveLocation::INLINE_FRAGMENT, nullptr);
                        inline_fragment->selection_set = parse_selection_set();
                        finish_syntax(inline_fragment);
                        selection_set->selections.push_back(inline_fragment);
                    }
                    else if (token == GraphQlToken::G_Name) {
                        auto fragment_spread = create_syntax<FragmentSpread>(SyntaxKind::S_FragmentSpread);
                        fragment_spread->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
                        forward_fragment_references.emplace_back(fragment_spread->name, current_object_types.top());
                        fragment_spread->directives = parse_directives(DirectiveLocation::FRAGMENT_SPREAD, nullptr);
                        selection_set->selections.push_back(fragment_spread);
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
                    auto fields = current_object_type->fields;
                    auto field_definition_it = fields->find(token_value);
                    if (field_definition_it == fields->end()) {
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
                        field->arguments = parse_arguments(field_definition_it->second->arguments, field);
                    }
                    field->directives = parse_directives(DirectiveLocation::FIELD, nullptr);
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
                        add_diagnostic(D::Expected_0_but_instead_reached_the_end_of_document, "}");
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
            const auto& members = static_cast<UnionTypeDefinition*>(current_object_type)->members;
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

    Arguments* GraphQlParser::parse_arguments(const std::map<Glib::ustring, InputValueDefinition*>& input_value_definitions, Field* field)
    {
        const auto& arguments = create_syntax<Arguments>(SyntaxKind::S_Arguments);
        while (true) {
            auto token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
            if (token != GraphQlToken::G_Name) {
                break;
            }
            auto token_value = get_token_value();
            auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
            auto argument = create_syntax<Argument>(SyntaxKind::S_Argument);
            argument->name = name;
            auto input_value_definition_it = input_value_definitions.find(token_value);
            bool is_defined = true;
            if (input_value_definition_it == input_value_definitions.end()) {
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
                argument->value = parse_value(input_value_definition_it->second->type);
            }
            else {
                scanner->skip_to({
                    GraphQlToken::G_Name,
                    GraphQlToken::CloseParen,
                });
            }
            arguments->arguments.push_back(argument);
        }
        finish_syntax(field);
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
        for (const auto& argument_definition : input_value_definitions) {
            const auto& type = argument_definition.second->type;
            if ((type->is_non_null && !type->is_list_type) || type->is_non_null_list) {
                if (parsed_arguments.find(argument_definition.second->name->identifier) == parsed_arguments.end()) {
                    add_diagnostic(
                        get_location_from_syntax(field),
                        D::Missing_required_argument_0,
                        argument_definition.second->name->identifier
                    );
                }
            }
        }
        return arguments;
    }

    std::map<Glib::ustring, Argument*>
    GraphQlParser::parse_arguments_after_open_paren()
    {
        std::map<Glib::ustring, Argument*> arguments;
        while (true) {
            auto token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
            if (token == GraphQlToken::CloseParen) {
                break;
            }
            auto token_value = get_token_value();
            auto name = create_syntax<Name>(SyntaxKind::S_Name, token_value);
            auto argument = create_syntax<Argument>(SyntaxKind::S_Argument);
            argument->name = name;
            if (!scan_expected(GraphQlToken::Colon)) {
                scanner->skip_to({ GraphQlToken::CloseBrace });
                return arguments;
            }
            argument->value = parse_value();
            const auto& argument_it = arguments.find(token_value);
            if (argument_it == arguments.end()) {
                arguments.emplace(token_value, argument);
            }
            else {
                if (!has_diagnostic_in_syntax(argument_it->second->name, D::Duplicate_argument_0)) {
                    add_diagnostic(get_location_from_syntax(argument_it->second->name), D::Duplicate_argument_0, token_value);
                }
                add_diagnostic(get_location_from_syntax(argument->name), D::Duplicate_argument_0, token_value);
            }
        }
        return arguments;
    }

    bool
    GraphQlParser::has_diagnostic_in_syntax(const Syntax* syntax, const DiagnosticMessageTemplate& diagnostic)
    {
        auto name_location = scanner->get_token_location(syntax);
        auto old_diagnostic_it = std::find_if(this->diagnostics.begin(), this->diagnostics.end(), [&](DiagnosticMessage& diagnostic_message) -> bool {
            auto location = diagnostic_message.location;
            if (diagnostic_message._template == diagnostic.message_template &&
                location.line == name_location.line &&
                location.column == name_location.column &&
                location.length == name_location.length) {
                return true;
            }
            return false;
        });
        if (old_diagnostic_it != this->diagnostics.end()) {
            return true;
        }
        return false;
    }

    std::map<Glib::ustring, Directive*>
    GraphQlParser::parse_directives(DirectiveLocation location, DirectiveDefinition* parent_directive_definition)
    {
        std::map<Glib::ustring, Directive*> directives = {};
        while (true) {
            if (!scan_optional(GraphQlToken::At)) {
                break;
            }
            GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/false);
            if (token != GraphQlToken::G_Name) {
                add_diagnostic(D::Expected_name_but_instead_got_0, graphQlTokenToString.at(token));
                skip_to_next_schema_primary_token();
                break;
            }
            auto directive = create_syntax<Directive>(SyntaxKind::S_Directive);
            directive->parent_directive_definition = parent_directive_definition;
            directive->start = directive->start - 1;
            directive->location = location;
            auto name = get_token_value();
            directive->name = create_syntax<Name>(SyntaxKind::S_Name, name);
            if (scan_optional(GraphQlToken::OpenParen)) {
                directive->arguments = parse_arguments_after_open_paren();
            }
            this->directives.push_back(directive);
            auto directive_name = directive->name->identifier;
            if (directives.find(directive_name) == directives.end()) {
                directives.emplace(directive_name, directive);
            }
            else {
                add_diagnostic(D::Duplicate_argument_0, directive_name);
            }
        }
        return directives;
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
    GraphQlParser::parse_value()
    {
        current_value_token = take_next_token();
        switch (current_value_token) {
            case GraphQlToken::OpenBracket: {
                const auto list_value = create_syntax<ListValue>(SyntaxKind::S_ListValue);
                Value* value = parse_value();
                while (value != nullptr) {
                    list_value->values.push_back(value);
                    value = parse_value();
                }
                if (current_value_token != GraphQlToken::CloseBracket) {
                    add_diagnostic(D::Expected_0_but_got_1, "]", get_token_value());
                }
                finish_syntax(list_value);
                return list_value;
            }
            case GraphQlToken::CloseBracket:
                return nullptr;

            case GraphQlToken::NullKeyword:
                return create_syntax<NullValue>(SyntaxKind::S_NullValue);
            case GraphQlToken::G_StringValue: {
                auto value = create_syntax<StringValue>(SyntaxKind::S_StringValue);
                value->value = get_token_value();
                return value;
            }
            case GraphQlToken::IntegerLiteral: {
                auto value = create_syntax<IntValue>(SyntaxKind::S_IntValue);
                value->value = std::atoi(get_token_value().c_str());
                return value;
            }
            case GraphQlToken::FloatLiteral: {
                auto value = create_syntax<FloatValue>(SyntaxKind::S_FloatValue);
                value->string = get_token_value();
                value->value = std::atof(value->string.c_str());
                return value;
            }
            case GraphQlToken::TrueKeyword: {
                auto boolean = create_syntax<BooleanValue>(SyntaxKind::S_BooleanValue);
                boolean->value = true;
                return boolean;
            }
            case GraphQlToken::FalseKeyword: {
                auto boolean = create_syntax<BooleanValue>(SyntaxKind::S_BooleanValue);
                boolean->value = false;
                return boolean;
            }
            case GraphQlToken::G_Name: {
                auto token_value = get_token_value();
                auto enum_value = create_syntax<EnumValue>(SyntaxKind::S_EnumValue);
                enum_value->value = token_value;
                return enum_value;
            }

//            case GraphQlToken::Dollar:
//                ArgumentLiteral argument_literal = create_syntax<ArgumentLiteral>(SyntaxKind::ArgumentLiteral);
//                argument_literal.name = parse_name();
//                finish_syntax(argument_literal);
//                return std::make_pair(argument_literal, ValueKind::Argument);
            case GraphQlToken::OpenBrace: {
                std::size_t start_position = scanner->start_position;
                auto location = get_token_location();
                auto object_value = create_syntax<ObjectValue>(SyntaxKind::S_ObjectValue);
                while (true) {
                    GraphQlToken token = take_next_token(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
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
                    auto object_field = create_syntax<ObjectField>(SyntaxKind::S_ObjectField);
                    object_field->name = create_syntax<Name>(SyntaxKind::S_Name, name);
                    if (!scan_expected(GraphQlToken::Colon)) {
                        return nullptr;
                    }
                    object_field->value = parse_value();
                    object_value->object_fields.push_back(object_field);
                }
                finish_syntax(object_value);
                return object_value;
            }
            default:
                add_diagnostic(D::Expected_value_instead_got_0, get_token_value());
                skip_to({
                    GraphQlToken::G_Name,
                    GraphQlToken::CloseParen,
                });
                return nullptr;
        }
    }

    void
    GraphQlParser::check_value(Value* value, Type* type) {
        switch (value->kind) {
            case SyntaxKind::S_NullValue:
                if ((!type->is_list_type && type->is_non_null) || type->is_non_null_list) {
                    add_diagnostic(get_location_from_syntax(value), D::The_value_0_is_not_assignable_to_type_1, "null", get_type_name(type));
                }
                break;

            case SyntaxKind::S_ListValue: {
                if (!type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                    return;
                }
                type->is_list_type = false;
                bool is_non_null_list = type->is_non_null_list;
                type->is_non_null_list = false;
                for (const auto& v : static_cast<ListValue*>(value)->values) {
                    check_value(v, type);
                }
                type->is_list_type = true;
                type->is_non_null_list = is_non_null_list;
                break;
            }

            case SyntaxKind::S_StringValue:
                if ((type->type & TypeEnum::T_StringType) == TypeEnum::T_None || type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                }
                break;

            case SyntaxKind::S_IntValue:
                if ((type->type & TypeEnum::T_IntegerType) == TypeEnum::T_None || type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                }
                break;

            case SyntaxKind::S_FloatValue:
                if (type->type != TypeEnum::T_Float || type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                }
                break;

            case SyntaxKind::S_BooleanValue:
                if (type->type != TypeEnum::T_Boolean || type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                }
                break;

            case SyntaxKind::S_ObjectValue:
                if (type->type != TypeEnum::T_Object || type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                }
                check_input_object(static_cast<ObjectValue*>(value), type);
                break;

            case SyntaxKind ::S_EnumValue:
                if (type->type != TypeEnum::T_Enum || type->is_list_type) {
                    add_diagnostic(get_location_from_syntax(value),
                        D::The_value_0_is_not_assignable_to_type_1,
                        get_value_string(value),
                        get_type_name(type)
                    );
                }
                break;

            default:;
        }
    }

    Glib::ustring
    GraphQlParser::get_value_string(Value* value)
    {
        TextWriter tw;
        switch (value->kind) {
            case SyntaxKind::S_ListValue:
                tw.write("[");
                for (const auto& value : static_cast<ListValue*>(value)->values) {
                    tw.write(get_value_string(value));
                    tw.save();
                    tw.write(", ");
                }
                tw.restore();
                tw.write("]");
                break;
            case SyntaxKind::S_BooleanValue:
                if (static_cast<BooleanValue*>(value)->value) {
                    tw.write("true");
                }
                else {
                    tw.write("false");
                }
                break;
            case SyntaxKind::S_NullValue:
                tw.write("null");
                break;
            case SyntaxKind::S_IntValue:
                tw.write(std::to_string(static_cast<IntValue*>(value)->value));
                break;
            case SyntaxKind::S_FloatValue:
                tw.write(static_cast<FloatValue*>(value)->string);
                break;
            case SyntaxKind::S_EnumValue:
                tw.write(static_cast<EnumValue*>(value)->value);
                break;
            case SyntaxKind::S_StringValue:
                tw.write(static_cast<StringValue*>(value)->value);
                break;
            case SyntaxKind::S_ObjectValue:
                tw.write("{");
                for (const auto& field : static_cast<ObjectValue*>(value)->object_fields) {
                    tw.write(field->name->identifier + ": ");
                    tw.write(get_value_string(field->value));
                    tw.save();
                    tw.write(",");
                }
                tw.restore();
                tw.write("}");
            default:
                throw std::logic_error("Unknown value.");
        }
        return *tw.text;
    }

    void
    GraphQlParser::check_input_object(ObjectValue* value, Type* type)
    {
        auto name = type->name->identifier;
        auto symbol_it = symbols.find(name);
        if (symbol_it == symbols.end()) {
            add_diagnostic(D::Type_0_is_not_defined, name);
            return;
        }
        if (symbol_it->second->kind != SymbolKind::SL_InputObject) {
            // A diagnostic should already be registered here.
            return;
        }
        InputObject* input_object = static_cast<InputObject*>(symbol_it->second->declaration);
        std::map<Glib::ustring, ObjectField*> checked_fields;
        for (const auto& field : value->object_fields) {
            const auto& field_name = field->name->identifier;
            const auto& field_it = input_object->fields->find(field_name);
            if (field_it == input_object->fields->end()) {
                add_diagnostic(
                    get_location_from_syntax(value),
                    D::Field_0_does_not_exist_on_type_1,
                    field->name->identifier,
                    input_object->name->identifier
                );
                continue;
            }
            auto checked_field_it = checked_fields.find(field_name);
            if (checked_field_it != checked_fields.end()) {
                add_diagnostic(
                    get_location_from_syntax(field),
                    D::Duplicate_field_0,
                    field->name->identifier
                );
                if (!has_diagnostic_in_syntax(checked_field_it->second, D::Duplicate_field_0)) {
                    add_diagnostic(
                        get_location_from_syntax(checked_field_it->second),
                        D::Duplicate_field_0,
                        field->name->identifier
                    );
                }
                continue;
            }
            checked_fields.emplace(field_name, field);
            check_value(field->value, field_it->second->type);
        }
        if (checked_fields.size() < input_object->fields->size()) {
            for (const auto& field_it : *input_object->fields) {
                const auto field = field_it.second;
                if ((field->type->is_non_null || field->type->is_non_null_list) && checked_fields.find(field_it.first) == checked_fields.end()) {
                    add_diagnostic(
                        get_location_from_syntax(value),
                        D::Missing_required_field_0,
                        field->name->identifier
                    );
                }
            }
        }
    }

    Glib::ustring
    GraphQlParser::get_token_value_from_syntax(Syntax* syntax)
    {
        return scanner->get_value_from_syntax(syntax);
    }

    Value*
    GraphQlParser::parse_value(Type* type)
    {
        current_value_token = take_next_token();
        switch (current_value_token) {
            case GraphQlToken::OpenBracket: {
                if (!type->is_list_type) {
                    add_diagnostic(
                        D::The_value_0_is_not_assignable_to_type_1,
                        "null",
                        get_type_name(type)
                    );
                }
                const auto list_value = create_syntax<ListValue>(SyntaxKind::S_ObjectValue);
                type->is_list_type = false;
                Value* value = parse_value(type);
                while (value != nullptr) {
                    list_value->values.push_back(value);
                    value = parse_value(type);
                }
                if (current_value_token != GraphQlToken::CloseBracket) {
                    add_diagnostic(D::Expected_0_but_got_1, "]", get_token_value());
                }
                type->is_list_type = true;
                finish_syntax(list_value);
                return list_value;
            }
            case GraphQlToken::CloseBracket:
                return nullptr;

            case GraphQlToken::NullKeyword:
                if (type->is_non_null || type->is_non_null_list) {
                    add_diagnostic(
                        D::The_value_0_is_not_assignable_to_type_1,
                        "null",
                        get_type_name(type)
                    );
                }
                return create_syntax<NullValue>(SyntaxKind::S_NullValue);
            case GraphQlToken::G_StringValue: {
                if ((type->type & TypeEnum::T_StringType) == TypeEnum::T_None || type->is_list_type) {
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
                if ((type->type & TypeEnum::T_IntegerType) == TypeEnum::T_None || type->is_list_type) {
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
                if (type->type != TypeEnum::T_Float || type->is_list_type) {
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
            case GraphQlToken::TrueKeyword:
                return parse_boolean_value(type, true);
            case GraphQlToken::FalseKeyword:
                return parse_boolean_value(type, false);
            case GraphQlToken::G_Name: {
                auto token_value = get_token_value();
                if (type->type != TypeEnum::T_Enum || type->is_list_type) {
                    add_diagnostic(
                        D::The_value_0_is_not_assignable_to_type_1,
                        token_value,
                        get_type_name(type)
                    );
                }
                auto _enum = static_cast<EnumTypeDefinition*>(symbols.find(type->name->identifier)->second->declaration);
                if (_enum->values.find(token_value) == _enum->values.end()) {
                    add_diagnostic(D::Undefined_enum_value_0, token_value);
                }
                auto enum_value = create_syntax<EnumValue>(SyntaxKind::S_EnumValue);
                enum_value->value = token_value;
                return enum_value;
            }

//            case GraphQlToken::Dollar:
//                ArgumentLiteral argument_literal = create_syntax<ArgumentLiteral>(SyntaxKind::ArgumentLiteral);
//                argument_literal.name = parse_name();
//                finish_syntax(argument_literal);
//                return std::make_pair(argument_literal, ValueKind::Argument);
            case GraphQlToken::OpenBrace: {
                std::size_t start_position = scanner->start_position;
                auto location = get_token_location();
                if (type->type != TypeEnum::T_Object || type->is_list_type) {
                    // TODO: Create a version of parse_value that doesn't do checking and proper object parsing
                    scanner->skip_block();
                    location.length += scanner->position - start_position - 1;
                    add_diagnostic(
                        location,
                        D::The_value_0_is_not_assignable_to_type_1,
                        scanner->get_text_from_location(start_position, scanner->position),
                        get_type_name(type)
                    );
                    return nullptr;
                }
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
                    const auto& field = input_object->fields->find(name);
                    if (field == input_object->fields->end()) {
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
                    object_field->value = parse_value(static_cast<Type*>(field->second->type));
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
                add_diagnostic(D::Expected_value_instead_got_0, get_token_value());
                skip_to({
                    GraphQlToken::G_Name,
                    GraphQlToken::CloseParen,
                });
                return nullptr;
        }
    }

    BooleanValue*
    GraphQlParser::parse_boolean_value(Type* type, bool value) {
        if (type->type != TypeEnum::T_Boolean || type->is_list_type) {
            add_diagnostic(
                D::Type_0_is_not_assignable_to_type_1,
                "Boolean",
                get_type_name(type)
            );
        }
        auto syntax = create_syntax<BooleanValue>(SyntaxKind::S_BooleanValue);
        syntax->value = value;
        return syntax;
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
        return scan_expected(token, treat_keyword_as_name, /*skip_white_space*/true);
    }


    inline
    bool
    GraphQlParser::scan_expected(const GraphQlToken& token, bool treat_keyword_as_name, bool skip_white_space)
    {
        GraphQlToken result = scanner->scan_expected(token, treat_keyword_as_name, skip_white_space);
        if (result != token) {
            if (result == GraphQlToken::EndOfDocument) {
                add_diagnostic(D::Expected_0_but_instead_reached_the_end_of_document, graphQlTokenToString.at(token));
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
        return scanner->take_next_token(false, true);
    }

    inline
    GraphQlToken
    GraphQlParser::take_next_token(bool treat_keyword_as_name)
    {
        return scanner->take_next_token(treat_keyword_as_name, true);
    }

    inline
    GraphQlToken
    GraphQlParser::take_next_token(bool treat_keyword_as_name, bool skip_white_space)
    {
        return scanner->take_next_token(treat_keyword_as_name, skip_white_space);
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
            case TypeEnum::T_Object:
                display_type += type->name->identifier;
                break;
            case TypeEnum::T_Enum:
                display_type += type->name->identifier;
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

    void
    GraphQlParser::skip_to_next_schema_primary_token()
    {
        scanner->skip_to({
            GraphQlToken::DirectiveKeyword,
            GraphQlToken::EnumKeyword,
            GraphQlToken::InputKeyword,
            GraphQlToken::InterfaceKeyword,
            GraphQlToken::SchemaKeyword,
            GraphQlToken::TypeKeyword,
            GraphQlToken::UnionKeyword,
        });
    }


    void
    GraphQlParser::skip_to_next_query_primary_token()
    {
        scanner->skip_to({
            GraphQlToken::QueryKeyword,
            GraphQlToken::MutationKeyword,
            GraphQlToken::SubscriptionKeyword,
            GraphQlToken::FragmentKeyword,
        });
    }

    bool
    GraphQlParser::token_is_primary(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::DirectiveKeyword:
            case GraphQlToken::EnumKeyword:
            case GraphQlToken::InputKeyword:
            case GraphQlToken::InterfaceKeyword:
            case GraphQlToken::SchemaKeyword:
            case GraphQlToken::TypeKeyword:
            case GraphQlToken::UnionKeyword:
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

    void
    GraphQlParser::take_errors_from_scanner()
    {
        while (!scanner->errors.empty()) {
            auto error = scanner->errors.top();
            diagnostics.push_back(error);
            scanner->errors.pop();
        }
    }
}
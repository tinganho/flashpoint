#include "graphql_executor.h"
//#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "graphql_diagnostics.h"
#include "lib/text_writer.h"
#include <memory>
#include <exception>

namespace flashpoint::program::graphql {

GraphQlExecutor::GraphQlExecutor(MemoryPool* memory_pool, MemoryPoolTicket* ticket):
    GraphQlParser(memory_pool, ticket)
{ }

ExecutableDefinition*
GraphQlExecutor::execute(const Glib::ustring& query)
{
    scanner = new GraphQlScanner(query);
    GraphQlToken token = take_next_token();
    auto executable_definition = create_syntax<ExecutableDefinition>(SyntaxKind::S_QueryDocument);
    executable_definition->source = &query;
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
//    check_directive_references();
    if (!diagnostics.empty()) {
        executable_definition->diagnostics = diagnostics;
    }
    return executable_definition;
}

void
GraphQlExecutor::add_schema(const GraphQlSchema& schema)
{
    schemas.push_back(schema);
    for (const auto& type_definition : schema.definitions) {
        if (type_definition->kind == SyntaxKind::S_Object) {

        }
    }
}

inline
void
GraphQlExecutor::check_forward_fragment_references(const std::vector<FragmentDefinition*>& fragment_definitions)
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

Syntax*
GraphQlExecutor::parse_executable_primary_token(GraphQlToken token)
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
GraphQlExecutor::parse_fragment_definition_after_fragment_keyword()
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
//    fragment->directives = parse_directives(DirectiveLocation::FRAGMENT_DEFINITION, nullptr);
    fragment->selection_set = parse_selection_set();
    if (fragment->selection_set == nullptr) {
        scanner->skip_block();
        return nullptr;
    }
    return fragment;
}

OperationDefinition* GraphQlExecutor::parse_operation_definition(const OperationType& operation_type)
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
//    operation_definition->directives = parse_directives(location, nullptr);
    return parse_operation_definition_body(operation_definition);
}

OperationDefinition*
GraphQlExecutor::parse_operation_definition_body(OperationDefinition* operation_definition)
{
    operation_definition->selection_set = parse_selection_set();
    if (operation_definition->selection_set == nullptr) {
        return nullptr;
    }
    return operation_definition;
}

OperationDefinition*
GraphQlExecutor::parse_operation_definition_body_after_open_brace(OperationDefinition* operation_definition)
{
    operation_definition->selection_set = parse_selection_set_after_open_brace();
    if (operation_definition->selection_set == nullptr) {
        return nullptr;
    }
    return operation_definition;
}

SelectionSet* GraphQlExecutor::parse_selection_set()
{
    if (!scan_expected(GraphQlToken::OpenBrace)) {
        return nullptr;
    }
    return parse_selection_set_after_open_brace();
}

SelectionSet* GraphQlExecutor::parse_selection_set_after_open_brace()
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
//                    inline_fragment->directives = parse_directives(DirectiveLocation::INLINE_FRAGMENT, nullptr);
                    inline_fragment->selection_set = parse_selection_set();
                    finish_syntax(inline_fragment);
                    selection_set->selections.push_back(inline_fragment);
                }
                else if (token == GraphQlToken::G_Name) {
                    auto fragment_spread = create_syntax<FragmentSpread>(SyntaxKind::S_FragmentSpread);
                    fragment_spread->name = create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
                    forward_fragment_references.emplace_back(fragment_spread->name, current_object_types.top());
//                    fragment_spread->directives = parse_directives(DirectiveLocation::FRAGMENT_SPREAD, nullptr);
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
//                field->directives = parse_directives(DirectiveLocation::FIELD, nullptr);
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
GraphQlExecutor::check_fragment_assignment(Name* name, Object* object, Declaration* current_object_type) {
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

Arguments* GraphQlExecutor::parse_arguments(const std::map<Glib::ustring, InputValueDefinition*>& input_value_definitions, Field* field)
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

Name* GraphQlExecutor::parse_optional_name()
{
    if (scan_optional(GraphQlToken::G_Name)) {
        return create_syntax<Name>(SyntaxKind::S_Name, get_token_value());
    }
    return nullptr;
}

VariableDefinitions* GraphQlExecutor::parse_variable_definitions()
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

Glib::ustring
GraphQlExecutor::get_token_value_from_syntax(Syntax* syntax)
{
    return scanner->get_value_from_syntax(syntax);
}

Value*
GraphQlExecutor::parse_value(Type* type)
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
GraphQlExecutor::parse_boolean_value(Type* type, bool value) {
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

Name*
GraphQlExecutor::parse_expected_name()
{
    if (!scan_expected(GraphQlToken::G_Name)) {
        return nullptr;
    }
    return create_syntax<Name>(SyntaxKind::S_Name, scanner->get_value().c_str());
}

Type*
GraphQlExecutor::parse_type()
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
GraphQlExecutor::current_position()
{
    return scanner->position;
}

void
GraphQlExecutor::skip_to_next_query_primary_token()
{
    scanner->skip_to({
        GraphQlToken::QueryKeyword,
        GraphQlToken::MutationKeyword,
        GraphQlToken::SubscriptionKeyword,
        GraphQlToken::FragmentKeyword,
    });
}

}
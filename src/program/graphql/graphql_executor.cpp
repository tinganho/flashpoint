#include "graphql_executor.h"
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
GraphQlExecutor::Execute(const Glib::ustring &query)
{
    scanner = new GraphQlScanner(query);
    GraphQlToken token = TakeNextToken();
    auto executableDefinition = CreateSyntax<ExecutableDefinition>(SyntaxKind::S_QueryDocument);
    executableDefinition->source = &query;
    while (token != GraphQlToken::EndOfDocument) {
        auto definition = parse_primary_token(token);
        if (definition == nullptr) {
            token = TakeNextToken();
            continue;
        }
        if (definition->kind == SyntaxKind::S_FragmentDefinition) {
            executableDefinition->fragment_definitions.push_back(static_cast<FragmentDefinition*>(definition));
        }
        else {
            executableDefinition->operation_definitions.push_back(static_cast<OperationDefinition*>(definition));
        }
        token = TakeNextToken();
    }
    check_forward_fragment_references(executableDefinition->fragment_definitions);
    if (!diagnostics.empty()) {
        executableDefinition->diagnostics = diagnostics;
    }
    return executableDefinition;
}

void
GraphQlExecutor::AddSchema(const GraphQlSchema &schema)
{
    schemas.push_back(schema);
    query = schema.query;
    mutation = schema.mutation;
    subscription = schema.subscription;
    symbols = schema.symbols;
    directive_definitions = schema.directive_definitions;
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
            AddDiagnostic(
                GetLocationFromSyntax(name),
                D::Fragment_0_is_not_defined,
                name->identifier
            );
            continue;
        }
        const auto& current_object_type = std::get<1>(forward_fragment_reference);
        auto symbols_it = symbols.find(current_fragment_definition->type->identifier);
        check_fragment_assignment(name, static_cast<Object*>(symbols_it->second->declaration), current_object_type);
    }
}

Syntax*
GraphQlExecutor::parse_primary_token(GraphQlToken token)
{
    switch (token) {
        case GraphQlToken::QueryKeyword: {
            current_object_types.push(query);
            return parse_operation_definition(OperationType::Query);
        }
        case GraphQlToken::MutationKeyword: {
            if (mutation == nullptr) {
                AddDiagnostic(D::Mutations_are_not_supported);
                skip_to_next_primary_token();
                return nullptr;
            }
            current_object_types.push(mutation);
            return parse_operation_definition(OperationType::Mutation);
        }
        case GraphQlToken::SubscriptionKeyword: {
            if (subscription == nullptr) {
                AddDiagnostic(D::Subscriptions_are_not_supported);
                skip_to_next_primary_token();
                return nullptr;
            }
            current_object_types.push(subscription);
            return parse_operation_definition(OperationType::Subscription);
        }
        case GraphQlToken::OpenBrace: {
            current_object_types.push(query);
            auto operation_definition = CreateSyntax<OperationDefinition>(SyntaxKind::S_OperationDefinition);
            operation_definition->operation_type = OperationType::Query;
            return parse_operation_definition_body_after_open_brace(operation_definition);
        }
        case GraphQlToken::FragmentKeyword:
            return parse_fragment_definition_after_fragment_keyword();
        default:
            AddDiagnostic(D::Expected_operation_definition_or_fragment_definition);
            return nullptr;
    }
}

FragmentDefinition*
GraphQlExecutor::parse_fragment_definition_after_fragment_keyword()
{
    auto fragment = CreateSyntax<FragmentDefinition>(SyntaxKind::S_FragmentDefinition);
    if (!ScanExpected(GraphQlToken::G_Name)) {
        return nullptr;
    }
    auto token_value = GetTokenValue();
    fragment->name = CreateSyntax<Name>(SyntaxKind::S_Name, token_value);
    const auto& fragments_it = fragments.find(token_value);
    if (fragments_it != fragments.end()) {
        AddDiagnostic(D::Duplicate_fragment_0, token_value);
        if (duplicate_fragments.find(token_value) == duplicate_fragments.end()) {
            AddDiagnostic(
                GetLocationFromSyntax(fragments_it->second->name),
                D::Duplicate_fragment_0,
                fragments_it->second->name->identifier
            );
        }
    }
    else {
        fragments.emplace(token_value, fragment);
    }
    if (!ScanExpected(GraphQlToken::OnKeyword)) {
        return nullptr;
    }
    if (!ScanExpected(GraphQlToken::G_Name)) {
        return nullptr;
    }
    const auto& name = GetTokenValue();
    const auto& it = symbols.find(name);
    if (it == symbols.end()) {
        AddDiagnostic(D::Type_0_is_not_defined, name);
        return nullptr;
    }
    const auto& object = static_cast<Object*>(it->second->declaration);
    if (object->kind != SyntaxKind::S_Object) {
        AddDiagnostic(D::The_type_0_is_not_an_object, name);
    }
    current_object_types.push(object);
    fragment->type = CreateSyntax<Name>(SyntaxKind::S_Name, name);
    fragment->directives = parse_directives(DirectiveLocation::FRAGMENT_DEFINITION);
    fragment->selection_set = parse_selection_set();
    if (fragment->selection_set == nullptr) {
        scanner->SkipBlock();
        return nullptr;
    }
    return fragment;
}

OperationDefinition*
GraphQlExecutor::parse_operation_definition(const OperationType& operation_type)
{
    auto operation_definition = CreateSyntax<OperationDefinition>(SyntaxKind::S_OperationDefinition);
    operation_definition->operation_type = operation_type;
    if (ScanOptional(GraphQlToken::G_Name, /*treat_keyword_as_name*/true)) {
        operation_definition->name = CreateSyntax<Name>(SyntaxKind::S_Name, GetTokenValue().c_str());
        if (ScanOptional(GraphQlToken::OpenParen)) {
            operation_definition->variable_definitions = parse_variable_definitions();
            ScanExpected(GraphQlToken::CloseParen);
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
    operation_definition->directives = parse_directives(location);
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
    if (!ScanExpected(GraphQlToken::OpenBrace)) {
        return nullptr;
    }
    return parse_selection_set_after_open_brace();
}

SelectionSet* GraphQlExecutor::parse_selection_set_after_open_brace()
{
    auto selection_set = CreateSyntax<SelectionSet>(SyntaxKind::S_SelectionSet);
    while (true) {
        std::size_t start_position = current_position();
        GraphQlToken token = TakeNextToken(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
        switch (token) {
            case GraphQlToken::Ellipses: {
                GraphQlToken target_token = TakeNextToken();
                if (target_token == GraphQlToken::OnKeyword) {
                    if (!ScanExpected(GraphQlToken::G_Name)) {
                        AddDiagnostic(D::Expected_name_of_object);
                        scanner->SkipBlock();
                        return nullptr;
                    }
                    const auto& token_value = GetTokenValue();
                    const auto& name = CreateSyntax<Name>(SyntaxKind::S_Name, token_value);
                    auto it = symbols.find(token_value);
                    if (it == symbols.end()) {
                        AddDiagnostic(D::Type_0_is_not_defined, token_value);
                        scanner->SkipBlock();
                        break;
                    }
                    auto inline_fragment = CreateSyntax<InlineFragment>(SyntaxKind::S_InlineFragment);
                    inline_fragment->start = start_position;
                    const auto& object = static_cast<Object*>(it->second->declaration);
                    if (object->kind != SyntaxKind::S_Object) {
                        AddDiagnostic(D::The_type_0_is_not_an_object, token_value);
                        scanner->SkipBlock();
                        break;
                    }
                    auto current_object_type = static_cast<Declaration*>(current_object_types.top());
                    check_fragment_assignment(name, object, current_object_type);
                    current_object_types.push(static_cast<ObjectLike*>(object));
                    inline_fragment->directives = parse_directives(DirectiveLocation::INLINE_FRAGMENT);
                    inline_fragment->selection_set = parse_selection_set();
                    FinishSyntax(inline_fragment);
                    selection_set->selections.push_back(inline_fragment);
                }
                else if (target_token == GraphQlToken::G_Name) {
                    auto fragment_spread = CreateSyntax<FragmentSpread>(SyntaxKind::S_FragmentSpread);
                    fragment_spread->name = CreateSyntax<Name>(SyntaxKind::S_Name, GetTokenValue());
                    forward_fragment_references.emplace_back(fragment_spread->name, current_object_types.top());
                    fragment_spread->directives = parse_directives(DirectiveLocation::FRAGMENT_SPREAD);
                    selection_set->selections.push_back(fragment_spread);
                }
                else {
                    AddDiagnostic(D::Expected_fragment_spread_or_inline_fragment);
                    scanner->SkipBlock();
                }
                break;
            }

            case GraphQlToken::G_Name: {
                auto token_value = GetTokenValue();
                auto current_object_type = current_object_types.top();
                auto fields = current_object_type->fields;
                auto field_definition_it = fields->find(token_value);
                if (field_definition_it == fields->end()) {
                    AddDiagnostic(D::Field_0_does_not_exist_on_type_1, token_value,
                                  current_object_type->name->identifier);
                    scanner->SkipTo({
                                        GraphQlToken::G_Name,
                                        GraphQlToken::CloseBrace,
                                        GraphQlToken::Ellipses,
                                    });
                    continue;
                }
                auto field = CreateSyntax<Field>(SyntaxKind::S_Field);
                auto name = CreateSyntax<Name>(SyntaxKind::S_Name, token_value);
                if (ScanOptional(GraphQlToken::Colon)) {
                    field->alias = name;
                    field->name = ParseExpectedName();
                }
                else {
                    field->name = name;
                }
                if (ScanOptional(GraphQlToken::OpenParen)) {
                    field->arguments = parse_arguments(field_definition_it->second->arguments, field);
                }
                field->directives = parse_directives(DirectiveLocation::FIELD);
                if (ScanOptional(GraphQlToken::OpenBrace)) {
                    if ((field_definition_it->second->type->type & TypeEnum::T_ObjectType) != TypeEnum::T_None) {
                        auto object = symbols.find(field_definition_it->second->type->name->identifier)->second->declaration;
                        current_object_types.push(static_cast<ObjectLike*>(object));
                        field->selection_set = parse_selection_set_after_open_brace();
                    }
                    else {
                        AddDiagnostic(D::Cannot_select_fields_on_a_scalar_field_0,
                                      GetTypeName(field_definition_it->second->type));
                        skip_to_next_primary_token();
                        return nullptr;
                    }
                }
                selection_set->selections.push_back(field);
                break;
            }

            case GraphQlToken::CloseBrace:
                current_object_types.pop();
                goto outer;

            case GraphQlToken::EndOfDocument: {
                if (selection_set->selections.size() == 0) {
                    AddDiagnostic(D::Expected_at_least_a_field_inline_fragment_or_fragment_spread);
                }
                else {
                    AddDiagnostic(D::Expected_0_but_instead_reached_the_end_of_document, "}");
                }
                return nullptr;
            }

            default:
                AddDiagnostic(D::Expected_field_inline_fragment_or_fragment_spread);
                goto outer;
        }
    }
    outer:
    FinishSyntax(selection_set);
    return selection_set;
}

void
GraphQlExecutor::check_fragment_assignment(Name* name, Object* object, Declaration* current_object_type) {
    if (current_object_type->kind == SyntaxKind::S_Interface) {
        if (object->implementations == nullptr || object->implementations->implementations.count(current_object_type->name->identifier) == 0) {
            AddDiagnostic(
                GetLocationFromSyntax(name),
                D::The_object_0_does_not_implement_the_interface_1,
                object->name->identifier,
                current_object_type->name->identifier
            );
        }
    }
    else if (current_object_type->kind == SyntaxKind::S_Object) {
        if (current_object_type->name->identifier != object->name->identifier) {
            AddDiagnostic(
                GetLocationFromSyntax(name),
                D::The_object_0_does_not_match_the_object_1,
                object->name->identifier,
                current_object_type->name->identifier
            );
        }
    }
    else if (current_object_type->kind == SyntaxKind::S_Union) {
        const auto& members = static_cast<UnionTypeDefinition*>(current_object_type)->members;
        if (members.find(object->name->identifier) == members.end()) {
            AddDiagnostic(
                GetLocationFromSyntax(name),
                D::The_type_0_is_not_member_of_the_union_1,
                object->name->identifier,
                current_object_type->name->identifier
            );
        }
    }
    else {
        AddDiagnostic(
            GetLocationFromSyntax(name),
            D::Type_conditions_are_only_applicable_inside_object_interfaces_and_unions
        );
    }
}

std::map<Glib::ustring, Argument*>*
GraphQlExecutor::parse_arguments(const std::map<Glib::ustring, InputValueDefinition*>& input_value_definitions, Syntax* target)
{
    auto arguments = new std::map<Glib::ustring, Argument*>();
    while (true) {
        auto token = TakeNextToken(/*treat_keyword_as_name*/true, /*skip_white_space*/true);
        if (token != GraphQlToken::G_Name) {
            break;
        }
        auto token_value = GetTokenValue();
        auto name = CreateSyntax<Name>(SyntaxKind::S_Name, token_value);
        auto argument = CreateSyntax<Argument>(SyntaxKind::S_Argument);
        argument->name = name;
        auto input_value_definition_it = input_value_definitions.find(token_value);
        bool is_defined = true;
        if (input_value_definition_it == input_value_definitions.end()) {
            AddDiagnostic(D::Unknown_argument_0, token_value);
            is_defined = false;
        }
        if (!ScanExpected(GraphQlToken::Colon)) {
            scanner->SkipTo({GraphQlToken::CloseBrace});
            return nullptr;
        }
        if (is_defined) {
            argument->value = parse_value(input_value_definition_it->second->type);
        }
        else {
            scanner->SkipTo({
                                GraphQlToken::G_Name,
                                GraphQlToken::CloseParen,
                            });
        }
        argument->directives = parse_directives(DirectiveLocation::ARGUMENT_DEFINITION);
        arguments->emplace(token_value, argument);
    }
    FinishSyntax(target);
    for (const auto& argument_definition : input_value_definitions) {
        const auto& type = argument_definition.second->type;
        if ((type->is_non_null && !type->is_list_type) || type->is_non_null_list) {
            if (arguments->find(argument_definition.second->name->identifier) == arguments->end()) {
                AddDiagnostic(
                    GetLocationFromSyntax(target),
                    D::Missing_required_argument_0,
                    argument_definition.second->name->identifier
                );
            }
        }
    }
    return arguments;
}

VariableDefinitions* GraphQlExecutor::parse_variable_definitions()
{
    auto variable_definitions = CreateSyntax<VariableDefinitions>(SyntaxKind::S_VariableDefinitions);
    while (true) {
        auto variable_definition = CreateSyntax<VariableDefinition>(SyntaxKind::S_VariableDefinition);
        GraphQlToken token = TakeNextToken();
        if (token != GraphQlToken::G_Name) {
            break;
        }
        variable_definition->name = CreateSyntax<Name>(SyntaxKind::S_Name, GetTokenValue().c_str());
        ScanExpected(GraphQlToken::Colon);
        variable_definition->type = parse_type();
        if (ScanOptional(GraphQlToken::Equal)) {
            variable_definition->default_value = parse_value(variable_definition->type);
        }
        if (!ScanOptional(GraphQlToken::Comma)) {
            break;
        }
        variable_definitions->variable_definitions.push_back(variable_definition);
    }
    return FinishSyntax(variable_definitions);
}

Value*
GraphQlExecutor::parse_value(Type* type)
{
    current_value_token = TakeNextToken();
    switch (current_value_token) {
        case GraphQlToken::OpenBracket: {
            if (!type->is_list_type) {
                AddDiagnostic(
                    D::The_value_0_is_not_assignable_to_type_1,
                    "null",
                    GetTypeName(type)
                );
            }
            const auto list_value = CreateSyntax<ListValue>(SyntaxKind::S_ObjectValue);
            type->is_list_type = false;
            Value* value = parse_value(type);
            while (value != nullptr) {
                list_value->values.push_back(value);
                value = parse_value(type);
            }
            if (current_value_token != GraphQlToken::CloseBracket) {
                AddDiagnostic(D::Expected_0_but_got_1, "]", GetTokenValue());
            }
            type->is_list_type = true;
            FinishSyntax(list_value);
            return list_value;
        }
        case GraphQlToken::CloseBracket:
            return nullptr;

        case GraphQlToken::NullKeyword:
            if (type->is_non_null || type->is_non_null_list) {
                AddDiagnostic(
                    D::The_value_0_is_not_assignable_to_type_1,
                    "null",
                    GetTypeName(type)
                );
            }
            return CreateSyntax<NullValue>(SyntaxKind::S_NullValue);
        case GraphQlToken::G_StringValue: {
            if ((type->type & TypeEnum::T_StringType) == TypeEnum::T_None || type->is_list_type) {
                AddDiagnostic(
                    D::Type_0_is_not_assignable_to_type_1,
                    "String",
                    GetTypeName(type)
                );
            }
            auto value = CreateSyntax<StringValue>(SyntaxKind::S_StringValue);
            value->value = GetTokenValue();
            return value;
        }
        case GraphQlToken::IntegerLiteral: {
            if ((type->type & TypeEnum::T_IntegerType) == TypeEnum::T_None || type->is_list_type) {
                AddDiagnostic(
                    D::Type_0_is_not_assignable_to_type_1,
                    "Int",
                    GetTypeName(type)
                );
            }
            auto value = CreateSyntax<IntValue>(SyntaxKind::S_IntValue);
            value->value = std::atoi(GetTokenValue().c_str());
            return value;
        }
        case GraphQlToken::FloatLiteral: {
            if (type->type != TypeEnum::T_Float || type->is_list_type) {
                AddDiagnostic(
                    D::Type_0_is_not_assignable_to_type_1,
                    "Float",
                    GetTypeName(type)
                );
            }
            auto value = CreateSyntax<FloatValue>(SyntaxKind::S_FloatValue);
            value->value = std::atof(GetTokenValue().c_str());
            return value;
        }
        case GraphQlToken::TrueKeyword:
            return parse_boolean_value(type, true);
        case GraphQlToken::FalseKeyword:
            return parse_boolean_value(type, false);
        case GraphQlToken::G_Name: {
            auto token_value = GetTokenValue();
            if (type->type != TypeEnum::T_Enum || type->is_list_type) {
                AddDiagnostic(
                    D::The_value_0_is_not_assignable_to_type_1,
                    token_value,
                    GetTypeName(type)
                );
            }
            auto _enum = static_cast<EnumTypeDefinition*>(symbols.find(type->name->identifier)->second->declaration);
            if (_enum->values.find(token_value) == _enum->values.end()) {
                AddDiagnostic(D::Undefined_enum_value_0, token_value);
            }
            auto enum_value = CreateSyntax<EnumValue>(SyntaxKind::S_EnumValue);
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
                scanner->SkipBlock();
                location.length += scanner->position - start_position - 1;
                AddDiagnostic(
                    location,
                    D::The_value_0_is_not_assignable_to_type_1,
                    scanner->get_text_from_location(start_position, scanner->position),
                    GetTypeName(type)
                );
                return nullptr;
            }
            auto object_value = CreateSyntax<ObjectValue>(SyntaxKind::S_ObjectValue);
            auto symbols_it = symbols.find(type->name->identifier);
            if (symbols_it->second->kind != SymbolKind::SL_InputObject) {
                AddDiagnostic(D::The_type_0_is_not_an_object, type->name->identifier);
                return nullptr;
            }
            const auto& input_object = static_cast<InputObject*>(symbols_it->second->declaration);
            std::vector<Glib::ustring> parsed_fields;
            while (true) {
                GraphQlToken token = TakeNextToken();
                if (token == GraphQlToken::CloseBrace) {
                    break;
                }
                if (token == GraphQlToken::EndOfDocument) {
                    break;
                }
                if (token != GraphQlToken::G_Name) {
                    AddDiagnostic(D::Expected_object_field_name);
                    auto t = scanner->SkipTo({GraphQlToken::CloseBrace, GraphQlToken::G_Name});
                    if (t == GraphQlToken::G_Name) {
                        continue;
                    }
                    break;
                }
                const auto& name = GetTokenValue();
                const auto& field = input_object->fields->find(name);
                if (field == input_object->fields->end()) {
                    AddDiagnostic(
                        D::Field_0_does_not_exist_on_type_1,
                        name,
                        type->name->identifier
                    );
                    SkipTo({
                               GraphQlToken::G_Name,
                               GraphQlToken::CloseBrace,
                           });
                    continue;
                }
                if (std::find(parsed_fields.begin(), parsed_fields.end(), name) != parsed_fields.end()) {
                    duplicate_fields.emplace(name);
                }
                parsed_fields.push_back(name);
                auto object_field = CreateSyntax<ObjectField>(SyntaxKind::S_ObjectField);
                object_field->name = CreateSyntax<Name>(SyntaxKind::S_Name, name);
                if (!ScanExpected(GraphQlToken::Colon)) {
                    return nullptr;
                }
                object_field->value = parse_value(static_cast<Type*>(field->second->type));
                object_value->object_fields.push_back(object_field);
            }
            FinishSyntax(object_value);
            if (type->type != TypeEnum::T_Object) {
                AddDiagnostic(
                    GetLocationFromSyntax(object_value),
                    D::The_value_0_is_not_assignable_to_type_1,
                    scanner->GetTextFromSyntax(object_value),
                    type->name->identifier
                );
            }

            if (duplicate_fields.size() > 0) {
                for (const auto& duplicate_field : duplicate_fields) {
                    for (const auto& object_field : object_value->object_fields) {
                        if (object_field->name->identifier == duplicate_field) {
                            AddDiagnostic(
                                GetLocationFromSyntax(object_field->name),
                                D::Duplicate_field_0,
                                duplicate_field
                            );
                        }
                    }
                }
            }

            for (const auto& required_field : input_object->required_fields) {
                if (std::find(parsed_fields.begin(), parsed_fields.end(), required_field) == parsed_fields.end()) {
                    AddDiagnostic(
                        GetLocationFromSyntax(object_value),
                        D::Missing_required_field_0,
                        required_field
                    );
                }
            }
            return object_value;
        }
        default:
            AddDiagnostic(D::Expected_value_instead_got_0, GetTokenValue());
            SkipTo({
                       GraphQlToken::G_Name,
                       GraphQlToken::CloseParen,
                   });
            return nullptr;
    }
}

BooleanValue*
GraphQlExecutor::parse_boolean_value(Type* type, bool value) {
    if (type->type != TypeEnum::T_Boolean || type->is_list_type) {
        AddDiagnostic(
            D::Type_0_is_not_assignable_to_type_1,
            "Boolean",
            GetTypeName(type)
        );
    }
    auto syntax = CreateSyntax<BooleanValue>(SyntaxKind::S_BooleanValue);
    syntax->value = value;
    return syntax;
}

std::map<Glib::ustring, Directive*>
GraphQlExecutor::parse_directives(DirectiveLocation location)
{
    std::map<Glib::ustring, Directive*> directives = {};
    while (true) {
        if (!ScanOptional(GraphQlToken::At)) {
            break;
        }
        GraphQlToken token = TakeNextToken(/*treat_keyword_as_name*/true, /*skip_white_space*/false);
        if (token != GraphQlToken::G_Name) {
            AddDiagnostic(D::Expected_name_but_instead_got_0, GetTokenValue());
            skip_to_next_primary_token();
            break;
        }
        auto directive = CreateSyntax<Directive>(SyntaxKind::S_Directive);
        directive->start = directive->start - 1;
        directive->location = location;
        auto name = GetTokenValue();
        auto definition = directive_definitions.find(name);
        if (definition == directive_definitions.end()) {
            AddDiagnostic(GetLocationFromSyntax(directive), D::Undefined_directive_0, name);
            SkipTo({GraphQlToken::CloseParen});
            return directives;
        }
        directive->name = CreateSyntax<Name>(SyntaxKind::S_Name, name);
        auto location_it = definition->second->locations.find(directive->location);
        if (location_it == definition->second->locations.end()) {
            AddDiagnostic(
                GetLocationFromSyntax(directive),
                D::The_directive_0_does_not_support_the_location_1,
                "@" + directive->name->identifier,
                directiveLocationToString.at(directive->location)
            );
        }
        directive->name = CreateSyntax<Name>(SyntaxKind::S_Name, name);
        if (ScanOptional(GraphQlToken::OpenParen)) {
            directive->arguments = parse_arguments(definition->second->arguments, directive->name);
            if (directive->arguments == nullptr) {
                return directives;
            }
        }
        else {
            directive->arguments = new std::map<Glib::ustring, Argument*>();
        }
        this->directives.push_back(directive);
        auto directive_name = directive->name->identifier;
        if (directives.find(directive_name) == directives.end()) {
            directives.emplace(directive_name, directive);
        }
        else {
            AddDiagnostic(D::Duplicate_argument_0, directive_name);
        }
    }
    return directives;
}

Name*
GraphQlExecutor::ParseExpectedName()
{
    if (!ScanExpected(GraphQlToken::G_Name)) {
        return nullptr;
    }
    return CreateSyntax<Name>(SyntaxKind::S_Name, scanner->GetValue().c_str());
}

Type*
GraphQlExecutor::parse_type()
{
    TypeEnum type;
    bool is_list_type = ScanOptional(GraphQlToken::OpenBracket);
    bool is_non_null_list = false;
    switch (TakeNextToken()) {
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
    bool is_non_null = ScanOptional(GraphQlToken::Exclamation);
    if (is_list_type) {
        ScanExpected(GraphQlToken::CloseBracket);
        is_non_null_list = ScanOptional(GraphQlToken::Exclamation);
    }
    return CreateSyntax<Type>(SyntaxKind::S_Type, type, is_non_null, is_list_type, is_non_null_list, false);
}

inline
std::size_t
GraphQlExecutor::current_position()
{
    return scanner->position;
}

void
GraphQlExecutor::skip_to_next_primary_token()
{
    scanner->SkipTo({
        GraphQlToken::QueryKeyword,
        GraphQlToken::MutationKeyword,
        GraphQlToken::SubscriptionKeyword,
        GraphQlToken::FragmentKeyword,
    });
}

}
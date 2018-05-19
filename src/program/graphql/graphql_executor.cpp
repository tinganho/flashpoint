#include "graphql_executor.h"
#include "graphql_scanner.h"
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
        auto schema_document = create_syntax<SchemaDocument>(SyntaxKind::QueryDocument);
        schema_document->source = schema;
        GraphQlToken token = next_token();
        while (token != GraphQlToken::EndOfDocument) {
            auto type_definition = parse_schema_primary_token(token);
            if (type_definition == nullptr) {
                break;
            }
            token = next_token();
        }
        check_forward_references();
        if (!diagnostics.empty()) {
            schema_document->diagnostics = diagnostics;
        }
        return schema_document;
    }

    void GraphQlParser::check_forward_references()
    {
        for (const auto& reference : forward_references) {
            if (object_types.find(reference->symbol) == object_types.end()) {
                add_diagnostic(get_location_from_syntax(reference), D::Type_0_is_not_defined, reference->symbol);
            }
        }
    }

    QueryDocument* GraphQlParser::execute(const Glib::ustring* query)
    {
        scanner = new GraphQlScanner(query);
        GraphQlToken token = next_token();
        auto query_document = create_syntax<QueryDocument>(SyntaxKind::QueryDocument);
        query_document->source = query;
        while (token != GraphQlToken::EndOfDocument) {
            auto operation_definition = parse_query_primary_token(token);
            if (operation_definition == nullptr) {
                break;
            }
            query_document->definitions.push_back(operation_definition);
            token = next_token();
        }
        if (!diagnostics.empty()) {
            query_document->diagnostics = diagnostics;
        }
        return query_document;
    }

    Syntax* GraphQlParser::parse_schema_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::TypeKeyword: {
                auto object = create_syntax<Object>(SyntaxKind::Object);
                if (!scan_expected(GraphQlToken::Name)) {
                    return nullptr;
                }
                auto token_value = get_token_value();
                object->name = create_syntax<Name>(SyntaxKind::Name, token_value);
                object->fields_definition = create_syntax<FieldsDefinition>(SyntaxKind::FieldsDefinition);
                if (!scan_expected(GraphQlToken::OpenBrace)) {
                    return nullptr;
                }
                auto& field_definitions = object->fields_definition->field_definitions;
                while (true) {
                    auto field_definition = create_syntax<FieldDefinition>(SyntaxKind::FieldDefinition);
                    GraphQlToken field_token = next_token();
                    if (field_token == GraphQlToken::EndOfDocument) {
                        return nullptr;
                    }
                    if (field_token == GraphQlToken::CloseBrace) {
                        break;
                    }
                    if (field_token != GraphQlToken::Name) {
                        add_diagnostic(D::Expected_field_definition);
                        return nullptr;
                    }
                    field_definition->name = create_syntax<Name>(SyntaxKind::Name, get_token_value());
                    if (scan_optional(GraphQlToken::OpenParen)) {
                        auto arguments_definition = nullptr;
                        if (!scan_expected(GraphQlToken::CloseParen)) {
                            return nullptr;
                        }
                    }
                    if (!scan_expected(GraphQlToken::Colon)) {
                        return nullptr;
                    }
                    field_definition->type = parse_type_definition();
                    if (field_definition->type == nullptr) {
                        return nullptr;
                    }
                    field_definitions.push_back(field_definition);
                    object->fields.emplace(field_definition->name->identifier, field_definition->type);
                }
                const auto& result = object_types.find(token_value);
                if (result == object_types.end()) {
                    object_types.emplace(token_value, object);
                }
                else {
                    add_diagnostic(get_location_from_syntax(result->second->name), D::Duplicate_type_0, result->second->name->identifier);
                    add_diagnostic(get_location_from_syntax(object->name), D::Duplicate_type_0, token_value);
                }
                return object;
            }
            default:
                add_diagnostic(D::Expected_operation_definition_or_fragment_definition);
                return nullptr;
        }
    }

    Type* GraphQlParser::parse_type_definition()
    {
        GraphQlToken token = next_token();
        auto type = create_syntax<Type>(SyntaxKind::Type);
        if (token == GraphQlToken::OpenBracket) {
            type->is_list_type = true;
            token = next_token();
        }
        switch(token) {
            case GraphQlToken::IntKeyword:
                type->type = TypeEnum::Int;
                break;
            case GraphQlToken::FloatKeyword:
                type->type = TypeEnum::Float;
                break;
            case GraphQlToken::StringKeyword:
                type->type = TypeEnum::String;
                break;
            case GraphQlToken::BooleanKeyword:
                type->type = TypeEnum::Boolean;
                break;
            case GraphQlToken::IDKeyword:
                type->type = TypeEnum::ID;
                break;
            case GraphQlToken::Name: {
                type->type = TypeEnum::Object;
                auto token_value = get_token_value();
                if (object_types.find(token_value) == object_types.end()) {
                    type->symbol = token_value;
                    forward_references.push_back(type);
                }
                break;
            }
            default:
                add_diagnostic(D::Expected_type_definition_instead_got_0, get_token_value());
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
                auto query = object_types.find("Query");
                if (query == object_types.end()) {
                    return nullptr;
                }
                current_object_type = query->second;
                return parse_operation_definition(OperationType::Query);
            }
            case GraphQlToken::MutationKeyword: {
                auto query = object_types.find("Mutation");
                if (query == object_types.end()) {
                    return nullptr;
                }
                current_object_type = query->second;
                return parse_operation_definition(OperationType::Mutation);
            }
            case GraphQlToken::SubscriptionKeyword: {
                auto query = object_types.find("Subscription");
                if (query == object_types.end()) {
                    return nullptr;
                }
                current_object_type = query->second;
                return parse_operation_definition(OperationType::Subscription);
            }
            case GraphQlToken::OpenBrace: {
                auto query = object_types.find("Query");
                if (query == object_types.end()) {
                    return nullptr;
                }
                current_object_type = query->second;
                return parse_operation_definition(OperationType::Query);
            }
            default:
                add_diagnostic(D::Expected_operation_definition_or_fragment_definition);
                return nullptr;
        }
    }

    OperationDefinition* GraphQlParser::parse_operation_definition(const OperationType& operation)
    {
        auto operation_definition = create_syntax<OperationDefinition>(SyntaxKind::OperationDefinition);
        if (scan_optional(GraphQlToken::Name)) {
            operation_definition->name = create_syntax<Name>(SyntaxKind::Name, get_token_value().c_str());
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
        return parse_selection_set_after_first_token();
    }

    SelectionSet* GraphQlParser::parse_selection_set_after_first_token()
    {
        auto selection_set = create_syntax<SelectionSet>(SyntaxKind::SelectionSet);
        while (true) {
            auto selection = create_syntax<Selection>(SyntaxKind::Selection);
            auto field = create_syntax<QueryField>(SyntaxKind::Field);
            GraphQlToken token = next_token();
            switch (token) {
                case GraphQlToken::Ellipses:
                    if (scan_optional(GraphQlToken::OnKeyword)) {
                    }
                    else {
                    }
                    break;

                case GraphQlToken::Name: {
                    auto token_value = get_token_value();
                    auto& fields = current_object_type->fields;
                    auto result = fields.find(token_value);
                    if (result == fields.end()) {
                        add_diagnostic(D::Field_0_doesnt_exist_on_type_1, token_value, current_object_type->name->identifier);
                        return nullptr;
                    }
                    auto name = create_syntax<Name>(SyntaxKind::Name, token_value);
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
                        field->selection_set = parse_selection_set_after_first_token();
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
        auto arguments = create_syntax<Arguments>(SyntaxKind::Arguments);
        while (true) {
            auto name = parse_optional_name();
            if (name != nullptr) {
                break;
            }
            auto argument = create_syntax<Argument>(SyntaxKind::Argument);
            argument->name = name;
            argument->value = parse_value();
        }
        auto name = parse_expected_name();
        return arguments;
    }

    Name* GraphQlParser::parse_optional_name()
    {
        if (scan_optional(GraphQlToken::Name)) {
            return create_syntax<Name>(SyntaxKind::Name, get_token_value());
        }
        return nullptr;
    }

    VariableDefinitions* GraphQlParser::parse_variable_definitions()
    {
        auto variable_definitions = create_syntax<VariableDefinitions>(SyntaxKind::VariableDefinitions);
        while (true) {
            auto variable_definition = create_syntax<VariableDefinition>(SyntaxKind::VariableDefinition);
            GraphQlToken token = next_token();
            if (token != GraphQlToken::Name) {
                break;
            }
            variable_definition->name = create_syntax<Name>(SyntaxKind::Name, get_token_value().c_str());
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
        switch (next_token()) {
            case GraphQlToken::StringLiteral:
                return create_syntax<StringLiteral>(SyntaxKind::StringLiteral, get_token_value());
            case GraphQlToken::IntLiteral:
                return create_syntax<IntLiteral>(SyntaxKind::IntLiteral, std::atoi(get_token_value().c_str()));
            case GraphQlToken::FloatLiteral:
                return create_syntax<FloatLiteral>(SyntaxKind::FloatLiteral, std::atof(get_token_value().c_str()));
            case GraphQlToken::NullKeyword:
                return create_syntax<NullLiteral>(SyntaxKind::NullLiteral);
            case GraphQlToken::BooleanLiteral:
                return create_syntax<BooleanLiteral>(SyntaxKind::BooleanLiteral, get_token_value().at(0) == 't');
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
        if (!scan_expected(GraphQlToken::Name)) {
            return nullptr;
        }
        return create_syntax<Name>(SyntaxKind::Name, scanner->get_value().c_str());
    }

    Type* GraphQlParser::parse_type()
    {
        TypeEnum type;
        bool is_list_type = scan_optional(GraphQlToken::OpenBracket);
        bool is_non_null_list = false;
        switch (next_token()) {
            case GraphQlToken::BooleanKeyword:
                type = TypeEnum::Boolean;
                break;
            case GraphQlToken::IntKeyword:
                type = TypeEnum::Int;
                break;
            case GraphQlToken::FloatKeyword:
                type = TypeEnum::Float;
                break;
            case GraphQlToken::StringKeyword:
                type = TypeEnum::String;
                break;
            case GraphQlToken::IDKeyword:
                type = TypeEnum::ID;
                break;
            default:
                type = TypeEnum::Object;
        }
        bool is_non_null = scan_optional(GraphQlToken::Exclamation);
        if (is_list_type) {
            scan_expected(GraphQlToken::CloseBracket);
            is_non_null_list = scan_optional(GraphQlToken::Exclamation);
        }
        return create_syntax<Type>(SyntaxKind::Type, type, is_non_null, is_list_type, is_non_null_list);
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

    inline bool GraphQlParser::scan_expected(const GraphQlToken& token, const char *token_name)
    {
        GraphQlToken result = scanner->scan_expected(token);
        if (result != token) {
            add_diagnostic(D::Expected_0_but_got_1, token_name, get_token_value());
            return false;
        }
        return true;
    }

    template<class T, class ... Args>
    T* GraphQlParser::create_syntax(SyntaxKind kind, Args ... args) {
        return new(memory_pool, ticket) T (kind, scanner->start_position, scanner->position, args...);
    }

    template<typename S>
    inline S* GraphQlParser::finish_syntax(S* syntax)
    {
        syntax->end = current_position() + 1;
        return syntax;
    }

    inline GraphQlToken GraphQlParser::next_token()
    {
        return scanner->next_token();
    }

    inline Glib::ustring GraphQlParser::get_token_value() const
    {
        return scanner->get_value();
    }

    inline GraphQlToken GraphQlParser::current_token() {
        return scanner->next_token();
    }
}
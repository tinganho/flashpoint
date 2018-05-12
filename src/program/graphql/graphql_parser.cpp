#include "graphql_parser.h"
#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include "graphql_diagnostics.h"
#include <memory>

namespace flashpoint::program::graphql {

    GraphQlParser::GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket):
        memory_pool(memory_pool),
        ticket(ticket)
    { }

    Document* GraphQlParser::parse(const Glib::ustring& text)
    {
        scanner = std::make_unique<GraphQlScanner>(text);
        GraphQlToken token = next_token();
        auto document = create_syntax<Document>(SyntaxKind::Document);
        while (token != GraphQlToken::EndOfDocument) {
            document->definitions.push_back(parse_primary_token(token));
            token = next_token();
        }
    }

    OperationDefinition* GraphQlParser::parse_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::QueryKeyword:
                return parse_operation_definition(OperationType::Query);
            case GraphQlToken::MutationKeyword:
                return parse_operation_definition(OperationType::Mutation);
            case GraphQlToken::SubscriptionKeyword:
                return parse_operation_definition(OperationType::Subscription);
            case GraphQlToken::OpenBrace:
                return parse_operation_definition(OperationType::Query);
            default:
                throw std::logic_error("Should not reach here.");
        }
    }

    OperationDefinition* GraphQlParser::parse_operation_definition(const OperationType &operation)
    {
        auto operation_definition = create_syntax<OperationDefinition>(SyntaxKind::OperationDefinition);
        if (try_scan(GraphQlToken::Name)) {
            operation_definition->name = create_syntax<Name>(SyntaxKind::Name, get_token_value().c_str());
            if (try_scan(GraphQlToken::OpenParen)) {
                operation_definition->variable_definitions = parse_variable_definitions();
                scan_expected(GraphQlToken::CloseParen);
            }
        }
        operation_definition->selection_set = parse_selection_set();
        return operation_definition;
    }

    SelectionSet* GraphQlParser::parse_selection_set()
    {
        scan_expected(GraphQlToken::OpenBrace);
        return parse_selection_set_after_first_token();
    }

    SelectionSet* GraphQlParser::parse_selection_set_after_first_token()
    {
        auto selection_set = create_syntax<SelectionSet>(SyntaxKind::SelectionSet);
        auto selection = create_syntax<Selection>(SyntaxKind::Selection);
        if (try_scan(GraphQlToken::Ellipses)) {
            if (try_scan(GraphQlToken::OnKeyword)) {
            }
            else {
            }
        }
        else {
            auto name_candidate = parse_expected_name("Field");
            auto field = create_syntax<Field>(SyntaxKind::Field);
            if (try_scan(GraphQlToken::Colon)) {
                field->alias = name_candidate;
                field->name = parse_expected_name();
            }
            else {
                field->name = name_candidate;
            }
            if (try_scan(GraphQlToken::OpenParen)) {
                field->arguments = parse_arguments();
            }
            if (try_scan(GraphQlToken::OpenBrace)) {
                field->selection_set = parse_selection_set_after_first_token();
            }
            selection->field = field;
            selection_set->selections.push_back(selection);
        }
        scan_expected(GraphQlToken::CloseBrace);
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
        if (try_scan(GraphQlToken::Name)) {
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
            if (try_scan(GraphQlToken::Equal)) {
                variable_definition->default_value = parse_value();
            }
            if (!try_scan(GraphQlToken::Comma)) {
                break;
            }
            variable_definitions->variable_definitions.push_back(variable_definition);
        }
        return finish_syntax(variable_definitions);
    }

    Value* GraphQlParser::parse_value()
    {
        switch (next_token()) {
            case GraphQlToken::StringLiteral:
                return new Value(
                    ValueType::String,
                    create_syntax<StringLiteral>(SyntaxKind::StringLiteral, get_token_value())
                );
            case GraphQlToken::IntLiteral:
                return new Value(
                    ValueType::Int,
                    create_syntax<IntLiteral>(
                        SyntaxKind::IntLiteral,
                        std::atoi(get_token_value().c_str())
                    )
                );
            case GraphQlToken::FloatLiteral:
                return new Value (
                    ValueType::Float,
                    create_syntax<FloatLiteral>(
                        SyntaxKind::IntLiteral,
                        std::atof(get_token_value().c_str())
                    )
                );
            case GraphQlToken::NullKeyword:
                return new Value (
                    ValueType::Null,
                    create_syntax<NullLiteral>(SyntaxKind::NullLiteral)
                );
            case GraphQlToken::BooleanLiteral:
                return new Value (
                    ValueType::Boolean,
                    create_syntax<BooleanLiteral>(SyntaxKind::BooleanLiteral, get_token_value().at(0) == 't')
                );
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

    SpanLocation GraphQlParser::get_token_location()
    {
        return SpanLocation {
            scanner->start_line,
            scanner->start_column,
            scanner->token_length,
            scanner->start_position,
        };
    }

    Name* GraphQlParser::parse_expected_name()
    {
        scan_expected(GraphQlToken::Name);
        return create_syntax<Name>(SyntaxKind::Name, scanner->get_value().c_str());
    }

    Name* GraphQlParser::parse_expected_name(const char* token_name)
    {
        scan_expected(GraphQlToken::Name, token_name);
        return create_syntax<Name>(SyntaxKind::Name, scanner->get_name());
    }

    Type* GraphQlParser::parse_type()
    {
        TypeEnum type;
        bool is_list = try_scan(GraphQlToken::OpenBracket);
        switch (next_token()) {
            case GraphQlToken::IntKeyword:
                type = TypeEnum::Int;
                break;
            case GraphQlToken::FloatKeyword:
                type = TypeEnum::Float;
                break;
            case GraphQlToken::StringKeyword:
                type = TypeEnum::String;
                break;
            case GraphQlToken::BooleanKeyword:
                type = TypeEnum::Boolean;
                break;
            case GraphQlToken::IDKeyword:
                type = TypeEnum::ID;
                break;
            default:
                type = TypeEnum::Object;
        }
        if (is_list) {
            scan_expected(GraphQlToken::CloseBracket);
        }
        bool is_nullable = try_scan(GraphQlToken::Exclamation);
        return create_syntax<Type>(SyntaxKind::Type, is_nullable, is_list, type);
    }

    unsigned int GraphQlParser::current_position()
    {
        return scanner->position;
    }

    inline bool GraphQlParser::try_scan(GraphQlToken token)
    {
        return scanner->try_scan(token);
    }

    inline void GraphQlParser::scan_expected(GraphQlToken token)
    {
        if (!scanner->scan_expected(token)) {
            add_diagnostic(D::Expected_0_but_got_1, scanner->get_value());
        }
    }

    inline void GraphQlParser::scan_expected(GraphQlToken token, const char* token_name)
    {
        if (!scanner->try_scan(token)) {
            add_diagnostic(D::Expected_0_but_got_1, token_name);
        }
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
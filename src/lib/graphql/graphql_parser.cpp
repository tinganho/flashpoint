#include "graphql_parser.h"
#include "graphql_scanner.h"
#include "graphql_syntaxes.h"
#include <memory>

namespace flashpoint::lib::graphql {

    void GraphQlParser::parse(const Glib::ustring& text)
    {
        scanner = std::make_unique<GraphQlScanner>(text);
        GraphQlToken token = next_token();
        while (token != GraphQlToken::EndOfRequestPayload) {
            parse_primary_token(token);
            token = next_token();
        }
    }

    Syntax GraphQlParser::parse_primary_token(GraphQlToken token)
    {
        switch (token) {
            case GraphQlToken::QueryKeyword:
                return parse_query(Operation::Query);
            case GraphQlToken::MutationKeyword:
                return parse_query(Operation::Mutation);
            case GraphQlToken::SubscriptionKeyword:
                return parse_query(Operation::Subscription);
            case GraphQlToken::OpenBrace:
                return parse_query(Operation::Query);
            default:
                throw std::logic_error("Should not reach here.");
        }
    }

    Query GraphQlParser::parse_query(const Operation& operation)
    {
        unsigned int start_position = current_position();
        std::experimental::optional<Signature> signature;
        if (scan_optional(GraphQlToken::Name)) {
            Name name = create_syntax<Name>(SyntaxKind::Name, get_token_value().c_str());
            std::experimental::optional<ParameterList> parameter_list;
            if (scan_optional(GraphQlToken::OpenParen)) {
                parameter_list = parse_parameter_list();
                scan_expected(GraphQlToken::CloseParen);
            }
            signature = create_syntax<Signature>(SyntaxKind::Signature, name, parameter_list);
        }
        return parse_query_body(operation, signature);
    }

    Query GraphQlParser::parse_query_body(const Operation& operation, const std::experimental::optional<Signature>& signature)
    {
        auto selection_set = parse_selection_set();
        return create_syntax<Query>(SyntaxKind::Query, operation, signature, selection_set);
    }

    SelectionSet GraphQlParser::parse_selection_set()
    {
        scan_expected(GraphQlToken::OpenBrace);
        auto selection_set = create_syntax<SelectionSet>(SyntaxKind::SelectionSet);

        finish_syntax(selection_set);
        scan_expected(GraphQlToken::CloseBrace);
    }

    ParameterList GraphQlParser::parse_parameter_list()
    {
        ParameterList parameter_list = create_syntax<ParameterList>(SyntaxKind::ParameterList);
        while (true) {
            GraphQlToken token = next_token();
            if (token != GraphQlToken::Name) {
                break;
            }
            Name name = create_syntax<Name>(SyntaxKind::Name, get_token_value().c_str());
            scan_expected(GraphQlToken::Colon);
            Type type = parse_type();
            std::experimental::optional<Value> default_value;
            if (scan_optional(GraphQlToken::Equal)) {
                default_value = parse_value();
            }
            if (!scan_optional(GraphQlToken::Comma)) {
                break;
            }
            parameter_list.parameters.emplace_back(SyntaxKind::Parameter, scanner->start_position, current_position(), name, type, default_value);
        }
        return finish_syntax(parameter_list);
    }

    Value GraphQlParser::parse_value()
    {
        switch (next_token()) {
            case GraphQlToken::StringLiteral:
                return Value(
                    ValueType::String,
                    create_syntax<StringLiteral>(SyntaxKind::StringLiteral, get_token_value())
                );
            case GraphQlToken::IntLiteral:
                return Value (
                    ValueType::Int,
                    create_syntax<IntLiteral>(
                        SyntaxKind::IntLiteral,
                        std::atoi(get_token_value().c_str())
                    )
                );
            case GraphQlToken::FloatLiteral: {
                return Value (
                    ValueType::Float,
                    create_syntax<FloatLiteral>(
                        SyntaxKind::IntLiteral,
                        std::atof(get_token_value().c_str())
                    )
                );
            }
            case GraphQlToken::NullKeyword:
                return Value (
                    ValueType::Null,
                    create_syntax<NullLiteral>(SyntaxKind::NullLiteral)
                );
            case GraphQlToken::BooleanLiteral:
                return Value (
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

    Name GraphQlParser::parse_name()
    {
        scan_expected(GraphQlToken::Name);
        return create_syntax<Name>(SyntaxKind::Name, scanner->get_value().c_str());
    }

    Type GraphQlParser::parse_type()
    {
        TypeEnum type;
        bool is_list = scan_optional(GraphQlToken::OpenBracket);
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
        bool is_nullable = scan_optional(GraphQlToken::Exclamation);
        return create_syntax<Type>(SyntaxKind::Type, is_nullable, is_list, type);
    }

    unsigned int GraphQlParser::current_position()
    {
        return scanner->position;
    }

    inline bool GraphQlParser::scan_optional(GraphQlToken token)
    {
        return scanner->scan_optional(token);
    }

    inline void GraphQlParser::scan_expected(GraphQlToken token)
    {
        scanner->scan_expected(token);
    }

    template<class T, class ... Args>
    inline T GraphQlParser::create_syntax(SyntaxKind kind, Args ... args) {
        return T (kind, scanner->start_position, scanner->position, args...);
    }

    template<typename S>
    inline S GraphQlParser::finish_syntax(S syntax)
    {
        syntax.end = current_position();
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
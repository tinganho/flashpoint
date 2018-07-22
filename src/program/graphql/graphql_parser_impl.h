#include "graphql_parser.h"
#include "graphql_syntaxes.h"
#include "graphql_diagnostics.h"
#include <lib/text_writer.h>
#include <program/diagnostic.h>

using namespace flashpoint::program::graphql;

template<typename TParser, typename TToken, typename TScanner>
GraphQlParser<TParser, TToken, TScanner>::GraphQlParser(const Glib::ustring& source, MemoryPool* memory_pool, MemoryPoolTicket* ticket):
    source(source),
    scanner(new GraphQlScanner(source)),
    memory_pool(memory_pool),
    ticket(ticket)
{ }


template<typename TParser, typename TToken, typename TScanner>
GraphQlParser<TParser, TToken, TScanner>::GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket):
    memory_pool(memory_pool),
    ticket(ticket)
{ }

template<typename TParser, typename TToken, typename TScanner>
TToken
GraphQlParser<TParser, TToken, TScanner>::take_next_token()
{
    return scanner->take_next_token(false, true);
}

template<typename TParser, typename TToken, typename TScanner>
inline
TToken
GraphQlParser<TParser, TToken, TScanner>::take_next_token(bool treat_keyword_as_name)
{
    return scanner->take_next_token(treat_keyword_as_name, true);
}

template<typename TParser, typename TToken, typename TScanner>
inline
TToken
GraphQlParser<TParser, TToken, TScanner>::take_next_token(bool treat_keyword_as_name, bool skip_white_space)
{
    return scanner->take_next_token(treat_keyword_as_name, skip_white_space);
}

template<typename TParser, typename TToken, typename TScanner>
inline
Glib::ustring
GraphQlParser<TParser, TToken, TScanner>::get_token_value() const
{
    return scanner->get_value();
}

template<typename TParser, typename TToken, typename TScanner>
Location
GraphQlParser<TParser, TToken, TScanner>::get_token_location()
{
    return Location {
        scanner->line,
        scanner->start_column,
        scanner->get_token_length(),
    };
}

template<typename TParser, typename TToken, typename TScanner>
Glib::ustring
GraphQlParser<TParser, TToken, TScanner>::get_type_name(Type *type)
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

template<typename TParser, typename TToken, typename TScanner>
inline
Glib::ustring
GraphQlParser<TParser, TToken, TScanner>::get_string_value() {
    return scanner->get_string_value();
}

template<typename TParser, typename TToken, typename TScanner>
Glib::ustring
GraphQlParser<TParser, TToken, TScanner>::get_value_string(Value* value)
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

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_expected(const TToken& token)
{
    return scan_expected(token, false);
}

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_expected(const TToken& token, bool treat_keyword_as_name)
{
    return scan_expected(token, treat_keyword_as_name, /*skip_white_space*/true);
}

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_expected(const TToken& token, bool treat_keyword_as_name, bool skip_white_space)
{
    TToken result = scanner->scan_expected(token, treat_keyword_as_name, skip_white_space);
    if (result != token) {
        if (result == TToken::EndOfDocument) {
            static_cast<TParser*>(this)->add_diagnostic(D::Expected_0_but_instead_reached_the_end_of_document, graphQlTokenToString.at(token));
        }
        else {
            static_cast<TParser*>(this)->add_diagnostic(D::Expected_0_but_got_1, graphQlTokenToString.at(token), static_cast<TParser*>(this)->get_token_value());
        }
        return false;
    }
    return true;
}

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_expected(const TToken& token, DiagnosticMessageTemplate& _template)
{
    TToken result = scanner->scan_expected(token);
    if (result != token) {
        static_cast<TParser*>(this)->add_diagnostic(_template, static_cast<TParser*>(this)->get_token_value());
        return false;
    }
    return true;
}

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_expected(const TToken& token, DiagnosticMessageTemplate& _template, bool treat_keyword_as_name)
{
    TToken result = scanner->scan_expected(token, treat_keyword_as_name);
    if (result != token) {
        static_cast<TParser*>(this)->add_diagnostic(_template, static_cast<TParser*>(this)->get_token_value());
        return false;
    }
    return true;
}

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_optional(const TToken& token)
{
    return scanner->try_scan(token) == token;
}

template<typename TParser, typename TToken, typename TScanner>
bool
GraphQlParser<TParser, TToken, TScanner>::scan_optional(const TToken& token, bool treat_keyword_as_name)
{
    return scanner->try_scan(token, treat_keyword_as_name) == token;
}

template<typename TParser, typename TToken, typename TScanner>
template<typename TSyntax>
Location
GraphQlParser<TParser, TToken, TScanner>::get_location_from_syntax(TSyntax* syntax)
{
    return scanner->get_token_location(syntax);
}

template<typename TParser, typename TToken, typename TScanner>
TToken
GraphQlParser<TParser, TToken, TScanner>::skip_to(const std::vector<TToken>& tokens)
{
    return scanner->skip_to(tokens);
}

template<typename TParser, typename TToken, typename TScanner>
template<typename TSyntax, typename TSyntaxKind, typename ... Args>
TSyntax*
GraphQlParser<TParser, TToken, TScanner>::create_syntax(TSyntaxKind kind, Args ... args) {
    return new (memory_pool, ticket) TSyntax (kind, scanner->start_position, scanner->position, args...);
}

template<typename TParser, typename TToken, typename TScanner>
template<typename TSyntax>
inline
TSyntax*
GraphQlParser<TParser, TToken, TScanner>::finish_syntax(TSyntax* syntax)
{
    syntax->end = scanner->start_position + scanner->length();
    return syntax;
}

template<typename TParser, typename TToken, typename TScanner>
template<typename TSyntax>
bool
GraphQlParser<TParser, TToken, TScanner>::has_diagnostic_in_syntax(TSyntax* syntax, const DiagnosticMessageTemplate& diagnostic)
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
    if (old_diagnostic_it != static_cast<TParser*>(this)->diagnostics.end()) {
        return true;
    }
    return false;
}

template<typename TParser, typename TToken, typename TScanner>
void
GraphQlParser<TParser, TToken, TScanner>::take_errors_from_scanner()
{
    while (!scanner->errors.empty()) {
        auto error = scanner->errors.top();
        this->diagnostics.push_back(error);
        scanner->errors.pop();
    }
}

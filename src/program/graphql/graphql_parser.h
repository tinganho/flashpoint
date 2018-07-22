#ifndef FLASHPOINT_GRAPHQL_PARSER_H
#define FLASHPOINT_GRAPHQL_PARSER_H

#include <program/diagnostic.h>
#include <lib/memory_pool.h>
#include <lib/types.h>
#include <glibmm/ustring.h>
#include "graphql_syntaxes.h"
#include "graphql_scanner.h"

using namespace flashpoint::lib;
using namespace flashpoint::program;
using namespace flashpoint::program::graphql;

template<typename TParser, typename TToken, typename TScanner>
class GraphQlParser : public DiagnosticTrait<TParser> {

public:
    GraphQlParser(const Glib::ustring& source, MemoryPool* memory_pool, MemoryPoolTicket* ticket);
    GraphQlParser(MemoryPool* memory_pool, MemoryPoolTicket* ticket);

    TScanner*
    scanner;

    const
    Glib::ustring
    source;

    Location
    get_token_location();

    MemoryPool*
    memory_pool;

    Glib::ustring
    current_description;

    GraphQlToken
    current_value_token;

    bool
    has_description;

    MemoryPoolTicket*
    ticket;

    std::vector<Directive*>
    directives;

    std::map<Glib::ustring, DirectiveDefinition*>
    directive_definitions;

    TToken
    take_next_token();

    TToken
    take_next_token(bool treat_keyword_as_name);

    TToken
    take_next_token(bool treat_keyword_as_name, bool skip_white_space);

    bool
    scan_expected(const TToken&);

    bool
    scan_expected(const TToken&, bool treat_keyword_as_name);

    bool
    scan_expected(const TToken&, bool treat_keyword_as_name, bool skip_white_space);

    bool
    scan_expected(const TToken& token, DiagnosticMessageTemplate& _template);

    bool
    scan_expected(const TToken& token, DiagnosticMessageTemplate& _template, bool treat_keyword_as_name);

    bool
    scan_optional(const TToken&);

    bool
    scan_optional(const TToken&, bool treat_keyword_as_name);

    bool
    scan_optional(const TToken&, bool treat_keyword_as_name, bool skip_white_space);

    Glib::ustring
    get_token_value() const;

    Location
    get_token_location() const;

    Glib::ustring
    get_string_value();

    TToken
    skip_to(const std::vector<TToken>& tokens);

    Glib::ustring
    get_type_name(Type* type);

    Glib::ustring
    get_value_string(Value* value);

    template<typename TSyntax, typename TSyntaxKind, typename ... Args>
    TSyntax*
    create_syntax(TSyntaxKind kind, Args ... args);

    template<typename TSyntax>
    TSyntax*
    finish_syntax(TSyntax* syntax);

    template<typename TSyntax>
    Location
    get_location_from_syntax(TSyntax* syntax);

    template<typename TSyntax>
    bool
    has_diagnostic_in_syntax(TSyntax* syntax, const DiagnosticMessageTemplate& diagnostic);

    void
    take_errors_from_scanner();
};

#include "graphql_parser_impl.h"

#endif //FLASHPOINT_GRAPHQL_PARSER_H

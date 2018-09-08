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
    GetTokenLocation() const;

    MemoryPool*
    memory_pool;

    MemoryPoolTicket*
    ticket;

    Glib::ustring
    current_description;

    GraphQlToken
    current_value_token;

    bool
    has_description;

    std::vector<Directive*>
    directives;

    std::map<Glib::ustring, DirectiveDefinition*>
    directive_definitions;

    TToken
    TakeNextToken();

    TToken
    TakeNextToken(bool treat_keyword_as_name);

    TToken
    TakeNextToken(bool treat_keyword_as_name, bool skip_white_space);

    bool
    ScanExpected(const TToken &);

    bool
    ScanExpected(const TToken&, bool treat_keyword_as_name);

    bool
    ScanExpected(const TToken&, bool treat_keyword_as_name, bool skip_white_space);

    bool
    ScanExpected(const TToken& token, DiagnosticMessageTemplate& _template);

    bool
    ScanExpected(const TToken& token, DiagnosticMessageTemplate& _template, bool treat_keyword_as_name);

    bool
    ScanOptional(const TToken &);

    bool
    ScanOptional(const TToken &, bool treat_keyword_as_name);

    bool
    ScanOptional(const TToken &, bool treat_keyword_as_name, bool skip_white_space);

    Glib::ustring
    GetTokenValue() const;

    Glib::ustring
    GetStringValue();

    TToken
    SkipTo(const std::vector<TToken> &tokens);

    Glib::ustring
    GetTypeName(Type *type);

    Glib::ustring
    GetValueString(Value *value);

    template<typename TSyntax, typename TSyntaxKind, typename ... Args>
    TSyntax*
    CreateSyntax(TSyntaxKind kind, Args ... args);

    template<typename TSyntax>
    TSyntax*
    FinishSyntax(TSyntax *syntax);

    template<typename TSyntax>
    Location
    GetLocationFromSyntax(TSyntax *syntax);

    template<typename TSyntax>
    bool
    HasDiagnosticInSyntax(TSyntax *syntax, const DiagnosticMessageTemplate &diagnostic);

    void
    TakeErrorsFromScanner();
};

#include "graphql_parser_impl.h"

#endif //FLASHPOINT_GRAPHQL_PARSER_H

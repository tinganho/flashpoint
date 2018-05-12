#ifndef FLASHPOINT_GRAPHQL_SYNTAXES_H
#define FLASHPOINT_GRAPHQL_SYNTAXES_H

#include <experimental/optional>
#include <glibmm/ustring.h>
#include <boost/variant.hpp>
#include <lib/memory_pool.h>
#include <vector>

#define C(name, ...) \
    name(SyntaxKind kind, unsigned int start, unsigned int end, __VA_ARGS__) noexcept: \
        Syntax(kind, start, end)

#define S(name) \
    name(SyntaxKind kind, unsigned int start, unsigned int end) noexcept: \
        Syntax(kind, start, end)

#define I(parameter) \
    parameter(parameter)

#define new_operator(class); \
    void* operator new(std::size_t size, MemoryPool* memory_pool, MemoryPoolTicket* request) { \
        return memory_pool->allocate(size, alignof(class), request); \
    }

using namespace flashpoint::lib;

namespace flashpoint::program::graphql {
    struct SelectionSet;

    enum class SyntaxKind {
        Argument,
        Arguments,
        Document,
        Name,
        NamedType,
        Field,
        OperationDefinition,
        Selection,
        SelectionSet,
        Type,
        TypeCondition,
        VariableDefinition,
        VariableDefinitions,

        // Literals
        IntLiteral,
        FloatLiteral,
        StringLiteral,
        BooleanLiteral,
        EnumLiteral,
        NullLiteral,

    };

    struct Syntax {
        SyntaxKind kind;
        unsigned int start;
        unsigned int end;

        Syntax(SyntaxKind kind, unsigned int start, unsigned int _end):
            kind(kind),
            start(start),
            end(_end)
        { }

        new_operator(Syntax)
    };

    struct Name : Syntax {
        Glib::ustring identifier;

        C(Name, Glib::ustring identifier),
            I(identifier) { }

        new_operator(Name)
    };

    struct NamedType {
        Name name;
    };

    enum class TypeEnum {
        Int,
        String,
        Float,
        Boolean,
        ID,
        Enum,
        Object,
    };

    struct Type : Syntax {
        bool is_nullable;
        bool is_list;
        TypeEnum type;
        std::experimental::optional<const char*> object_symbol;

        C(Type, bool is_nullable, bool is_list, TypeEnum type),
            I(is_nullable),
            I(is_list),
            I(type)
        { }

        C(Type, bool is_nullable, bool is_list, TypeEnum type, std::experimental::optional<const char*> object_symbol),
            I(is_nullable),
            I(is_list),
            I(type),
            I(object_symbol)
        { }
    };

    struct TypeCondition : Syntax {
        S(TypeCondition)
        { }
    };

    struct NullLiteral : Syntax {
        S(NullLiteral)
        { }
    };

    struct IntLiteral : Syntax {
        int value;

        C(IntLiteral, int value),
            I(value)
        { }
    };

    struct FloatLiteral : Syntax {
        double value;

        C(FloatLiteral, double value),
            I(value)
        { }
    };

    struct StringLiteral : Syntax {
        Glib::ustring value;

        C(StringLiteral, Glib::ustring  value),
            I(value)
        { }
    };

    struct BooleanLiteral : Syntax {
        bool value;

        C(BooleanLiteral, bool value),
            I(value)
        { }
    };

    struct ObjectLiteral : Syntax {
        bool value;

        C(ObjectLiteral, bool value),
            I(value)
        { }
    };

    struct EnumLiteral : Syntax {
        const char* value;

        C(EnumLiteral, const char* value),
            I(value)
        { }
    };

    struct ArgumentLiteral : Syntax {
        Name name;
        const char* symbol;

        C(ArgumentLiteral, Name name, const char* symbol),
            I(name),
            I(symbol)
        { }
    };

    enum class ValueType {
        None,
        Null,
        Int,
        Float,
        String,
        Boolean,
        Enum,
        Object,
        Argument,
    };

    typedef boost::variant<
        IntLiteral*,
        FloatLiteral*,
        BooleanLiteral*,
        StringLiteral*,
        NullLiteral*> Literal;

    struct Value {
        ValueType type;
        Literal value;

        Value(ValueType type, Literal value) noexcept:
            type(type),
            value(value)
        { }
    };


    struct VariableDefinition : Syntax {
        Name* name;
        Type* type;
        Value* default_value;

        S(VariableDefinition)
        { }
    };

    struct VariableDefinitions : Syntax {
        std::vector<VariableDefinition*> variable_definitions;

        S(VariableDefinitions)
        { }
    };

    struct Argument : Syntax {
        Name* name;
        Value* value;

        S(Argument)
        { }
    };

    struct Arguments : Syntax {
        std::vector<Argument> arguments;

        S(Arguments)
        { }
    };

    struct Directive : Syntax {
        Name* name;
        Arguments* arguments;

        S(Directive)
        { }
    };

    struct Directives : Syntax {
        std::vector<Directive> directives;

        S(Directives)
        { }
    };

    struct Field : Syntax {
        Name* alias;
        Name* name;
        Arguments* arguments;
        Directive* directive;
        SelectionSet* selection_set;

        S(Field)
        { }

        new_operator(Field)
    };

    struct FragmentSpread : Syntax {
        Name* name;
        Directives* directives;

        S(FragmentSpread)
        { }

        new_operator(FragmentSpread)
    };

    struct InlineFragment : Syntax {
        TypeCondition type_condition;

        C(InlineFragment, TypeCondition type_condition),
            I(type_condition)
        { }
    };

    typedef boost::variant<
        Field*,
        FragmentSpread*,
        InlineFragment*
    > OneOfFields;

    struct Selection : Syntax {
        OneOfFields field;

        S(Selection)
        { }

        new_operator(Selection)
    };

    struct SelectionSet : Syntax {
        std::vector<Selection*> selections;

        S(SelectionSet)
        { }

        new_operator(SelectionSet)
    };

    enum class OperationType {
        Query,
        Mutation,
        Subscription,
    };

    struct OperationDefinition : Syntax {
        OperationType operation_type;
        Name* name;
        VariableDefinitions* variable_definitions;
        SelectionSet* selection_set;

        S(OperationDefinition)
        { }

        new_operator(OperationDefinition)
    };

    struct FragmentDefinition : Syntax {
        Name* name;
        TypeCondition* type_condition;
        Directives* directives;
        SelectionSet* selection_set;

        S(FragmentDefinition)
        { }

        new_operator(FragmentDefinition)
    };

    typedef boost::variant<OperationDefinition*, FragmentDefinition*> Definition;

    struct Document : Syntax {
        std::vector<Definition> definitions;

        S(Document)
        {}
    };
};

#endif //FLASHPOINT_GRAPHQL_SYNTAXES_H

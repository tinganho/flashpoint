#ifndef FLASHPOINT_GRAPHQL_SYNTAXES_H
#define FLASHPOINT_GRAPHQL_SYNTAXES_H

#include <experimental/optional>
#include <glibmm/ustring.h>
#include <boost/variant.hpp>
#include <vector>

#define C(name, ...) \
    name(SyntaxKind kind, unsigned int start, unsigned int end, __VA_ARGS__) noexcept: \
        Syntax(kind, start, end)

#define S(name) \
    name(SyntaxKind kind, unsigned int start, unsigned int end) noexcept: \
        Syntax(kind, start, end)

#define I(parameter) \
    parameter(parameter)

namespace flashpoint::lib::graphql {

    enum class SyntaxKind {
        Name,
        NamedType,
        Parameter,
        ParameterList,

        // Literals
        IntLiteral,
        FloatLiteral,
        StringLiteral,
        BooleanLiteral,
        EnumLiteral,
        NullLiteral,
        ArgumentLiteral,


        Type,
        TypeCondition,
        Selection,
        SelectionSet,
        Signature,
        Query,
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
    };

    struct Name : Syntax {
        const char* identifier;

        C(Name, const char* identifier),
            I(identifier) { }
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

        // Only set if type is object, otherwise empty
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
        IntLiteral,
        FloatLiteral,
        BooleanLiteral,
        StringLiteral,
        NullLiteral> Literal;

    struct Value {
        ValueType type;
        Literal value;

        Value(ValueType type, Literal value) noexcept:
            type(type),
            value(value)
        { }
    };

    struct Parameter : Syntax {
        Name name;
        Type type;
        std::experimental::optional<Value> default_value;

        C(Parameter, Name name, Type type, std::experimental::optional<Value> default_value),
            I(name),
            I(type),
            I(default_value)
        { }
    };

    struct ParameterList : Syntax {
        std::vector<Parameter> parameters;

        S(ParameterList)
        { }
    };

    struct Signature : Syntax {
        Name name;
        std::experimental::optional<ParameterList> parameter_list;

        C(Signature, Name name, std::experimental::optional<ParameterList> parameter_list),
            I(name),
            I(parameter_list)
        { }
    };

    struct InlineFragment : Syntax {
        TypeCondition type_condition;

        C(InlineFragment, TypeCondition type_condition),
            I(type_condition)
        { }
    };

    struct Selection : Syntax {
        enum Type { FIELD, FRAGMENTSPREAD, INLINEFRAGMENT } type;
        union Sel {
            InlineFragment fragment;
        } selection;

        C(Selection, Type type, Sel selection),
            I(type),
            I(selection)
        { }
    };

    struct SelectionSet : Syntax {
        std::vector<Selection> selections;

        S(SelectionSet)
        { }
    };

    enum class Operation {
        Query,
        Mutation,
        Subscription,
    };

    struct Query : Syntax {
        Operation operation;
        std::experimental::optional<Signature> signature;
        SelectionSet selection_set;

        C(Query, Operation operation, std::experimental::optional<Signature> signature, SelectionSet selection_set),
            I(operation),
            I(signature),
            I(selection_set)
        { }
    };

    struct RequestPayload {
        std::unique_ptr<Query> query;
    };
};

#endif //FLASHPOINT_GRAPHQL_SYNTAXES_H

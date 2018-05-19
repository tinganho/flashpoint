#ifndef FLASHPOINT_GRAPHQL_SYNTAXES_H
#define FLASHPOINT_GRAPHQL_SYNTAXES_H

#include <experimental/optional>
#include <program/diagnostic.h>
#include <glibmm/ustring.h>
#include <boost/variant.hpp>
#include <lib/memory_pool.h>
#include <vector>
#include <set>

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
    struct Type;
    struct Directives;
    class GraphQlSyntaxVisitor;

    enum class SyntaxKind {
        Argument,
        Arguments,
        Name,
        NamedType,
        Field,
        Object,
        OperationDefinition,
        QueryDocument,
        SchemaDocument,
        Selection,
        SelectionSet,
        Type,
        TypeCondition,
        VariableDefinition,
        VariableDefinitions,

        FieldsDefinition,
        FieldDefinition,

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

        virtual void accept(class GraphQlSyntaxVisitor*) const = 0;
    };

    struct Reference {
        Glib::ustring symbol;
    };

    struct Name : Syntax {
        Glib::ustring identifier;

        C(Name, Glib::ustring identifier),
            I(identifier) { }

        new_operator(Name)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct InputValueDefinition : Syntax {
        Glib::ustring* description;
        Name* name;
        Type* type;
        Syntax* default_value;
        Directives* directives;

        S(InputValueDefinition)
        { }

        new_operator(InputValueDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FieldDefinition : Syntax {
        Glib::ustring* description;
        Name* name;
        Type* type;

        S(FieldDefinition)
        { }

        new_operator(FieldDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FieldsDefinition : Syntax {
        std::vector<FieldDefinition*> field_definitions;

        S(FieldsDefinition)
        { }

        new_operator(FieldsDefinition)

        void accept(GraphQlSyntaxVisitor*) const override;
    };

    struct Object : Syntax {
        Name* name;
        FieldsDefinition* fields_definition;
        std::map<Glib::ustring, Syntax*> fields;

        S(Object)
        { }

        new_operator(Object)

        void accept(GraphQlSyntaxVisitor*) const;
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

    struct Type : Syntax, Reference {
        TypeEnum type;
        bool is_non_null;
        bool is_list_type;
        bool is_non_null_list;


        S(Type)
        { }

        C(Type, TypeEnum type, bool is_non_null, bool is_list_type, bool is_non_null_list),
            I(type),
            I(is_non_null),
            I(is_list_type),
            I(is_non_null_list)
        { }

        new_operator(Type)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct NamedType : Syntax {
        Name* name;

        S(NamedType)
        { }

        new_operator(NamedType)
    };

    struct TypeCondition : Syntax {
        S(TypeCondition)
        { }

        new_operator(TypeCondition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct NullLiteral : Syntax {
        S(NullLiteral)
        { }

        new_operator(NullLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct IntLiteral : Syntax {
        int value;

        C(IntLiteral, int value),
            I(value)
        { }

        new_operator(IntLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FloatLiteral : Syntax {
        double value;

        C(FloatLiteral, double value),
            I(value)
        { }

        new_operator(FloatLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct StringLiteral : Syntax {
        Glib::ustring value;

        C(StringLiteral, Glib::ustring  value),
            I(value)
        { }

        new_operator(StringLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct BooleanLiteral : Syntax {
        bool value;

        C(BooleanLiteral, bool value),
            I(value)
        { }

        new_operator(BooleanLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct ObjectLiteral : Syntax {
        bool value;

        C(ObjectLiteral, bool value),
            I(value)
        { }

        new_operator(ObjectLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct EnumLiteral : Syntax {
        const char* value;

        C(EnumLiteral, const char* value),
            I(value)
        { }

        new_operator(EnumLiteral)

        void accept(GraphQlSyntaxVisitor*) const;
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

    struct Value : Syntax {
        Syntax* value;

        C(Value, Syntax* value),
            I(value)
        { }

        new_operator(Value)

        void accept(GraphQlSyntaxVisitor*) const;
    };


    struct VariableDefinition : Syntax {
        Name* name;
        Type* type;
        Syntax* default_value;

        S(VariableDefinition)
        { }

        new_operator(VariableDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct VariableDefinitions : Syntax {
        std::vector<VariableDefinition*> variable_definitions;

        S(VariableDefinitions)
        { }

        new_operator(VariableDefinitions)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Argument : Syntax {
        Name* name;
        Syntax* value;

        S(Argument)
        { }

        new_operator(Argument)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Arguments : Syntax {
        std::vector<Argument> arguments;

        S(Arguments)
        { }

        new_operator(Arguments)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Directive : Syntax {
        Name* name;
        Arguments* arguments;

        S(Directive)
        { }

        new_operator(Directive)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Directives : Syntax {
        std::vector<Directive> directives;

        S(Directives)
        { }

        new_operator(Directives)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct QueryField : Syntax {
        Name* alias;
        Name* name;
        Arguments* arguments;
        Directive* directive;
        SelectionSet* selection_set;

        S(QueryField)
        { }

        new_operator(QueryField)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FragmentSpread : Syntax {
        Name* name;
        Directives* directives;

        S(FragmentSpread)
        { }

        new_operator(FragmentSpread)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct InlineFragment : Syntax {
        TypeCondition* type_condition;

        S(InlineFragment)
        { }

        new_operator(InlineFragment)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Selection : Syntax {
        Syntax* field;

        S(Selection)
        { }

        new_operator(Selection)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct SelectionSet : Syntax {
        std::vector<Selection*> selections;

        S(SelectionSet)
        { }

        new_operator(SelectionSet)

        void accept(GraphQlSyntaxVisitor*) const;
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

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FragmentDefinition : Syntax {
        Name* name;
        TypeCondition* type_condition;
        Directives* directives;
        SelectionSet* selection_set;

        S(FragmentDefinition)
        { }

        new_operator(FragmentDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct QueryDocument : Syntax {
        std::vector<Syntax*> definitions;
        std::vector<DiagnosticMessage> diagnostics;
        const Glib::ustring* source;

        S(QueryDocument)
        { }

        new_operator(QueryDocument)

        void accept(GraphQlSyntaxVisitor*) const;
    };


    struct SchemaDocument : Syntax {
        std::vector<Syntax*> definitions;
        std::vector<DiagnosticMessage> diagnostics;
        const Glib::ustring* source;

        S(SchemaDocument)
        { }

        new_operator(SchemaDocument)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    class GraphQlSyntaxVisitor {
    public:
        virtual void visit(const Name*) = 0;
        virtual void visit(const Value*) = 0;
        virtual void visit(const Type*) = 0;
        virtual void visit(const IntLiteral*) = 0;
        virtual void visit(const NullLiteral*) = 0;
        virtual void visit(const FloatLiteral*) = 0;
        virtual void visit(const StringLiteral*) = 0;
        virtual void visit(const BooleanLiteral*) = 0;
        virtual void visit(const VariableDefinition*) = 0;
        virtual void visit(const VariableDefinitions*) = 0;
        virtual void visit(const Argument*) = 0;
        virtual void visit(const Arguments*) = 0;
        virtual void visit(const QueryField*) = 0;
        virtual void visit(const SelectionSet*) = 0;
        virtual void visit(const Selection*) = 0;
        virtual void visit(const OperationDefinition*) = 0;
        virtual void visit(const FragmentDefinition*) = 0;
        virtual void visit(const QueryDocument*) = 0;

        virtual void visit(const SchemaDocument*) = 0;
        virtual void visit(const Object*) = 0;
        virtual void visit(const FieldDefinition*) = 0;
        virtual void visit(const FieldsDefinition*) = 0;
    };
};

#endif //FLASHPOINT_GRAPHQL_SYNTAXES_H

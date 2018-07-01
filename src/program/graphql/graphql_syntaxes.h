#ifndef FLASHPOINT_GRAPHQL_SYNTAXES_H
#define FLASHPOINT_GRAPHQL_SYNTAXES_H

#include <experimental/optional>
#include <program/diagnostic.h>
#include <glibmm/ustring.h>
#include <boost/variant.hpp>
#include <lib/memory_pool.h>
#include <vector>
#include <set>
#include <stack>

#define C(name, ...) \
    name(SyntaxKind kind, unsigned int start, unsigned int end, __VA_ARGS__) noexcept: \
        Syntax(kind, start, end)

#define D(name, base) \
    name(SyntaxKind kind, unsigned int start, unsigned int end) noexcept: \
        base(kind, start, end)

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
    struct Directive;
    struct Directives;
    struct Interface;
    class GraphQlSyntaxVisitor;

    enum class SyntaxKind {
        S_None,
        S_Argument,
        S_Arguments,
        S_ArgumentsDefinition,
        S_Directive,
        S_DirectiveDefinition,
        S_EnumTypeDefinition,
        S_FieldsDefinition,
        S_FieldDefinition,
        S_FragmentDefinition,
        S_FragmentSpread,
        S_InlineFragment,
        S_InputFieldsDefinition,
        S_InputFieldDefinition,
        S_InputValueDefinition,
        S_InputObject,
        S_Interface,
        S_Implementations,
        S_Name,
        S_Field,
        S_Object,
        S_OperationDefinition,
        S_QueryDocument,
        S_Schema,
        S_SchemaDocument,
        S_SelectionSet,
        S_Type,
        S_Union,
        S_VariableDefinition,
        S_VariableDefinitions,

        // Literals
        S_NullValue,
        S_BooleanValue,
        S_IntValue,
        S_FloatValue,
        S_EnumValue,
        S_StringValue,
        S_ObjectField,
        S_ObjectValue,
        S_ListValue,

        S_InputTypeWithDirectives = static_cast<int>(S_InputObject) | static_cast<int>(S_EnumTypeDefinition),
        S_InputType = static_cast<int>(S_InputObject) | static_cast<int>(S_EnumTypeDefinition),
    };

    inline constexpr SyntaxKind operator|(SyntaxKind a, SyntaxKind b) {
        return static_cast<SyntaxKind>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    inline constexpr SyntaxKind operator&(SyntaxKind a, SyntaxKind b) {
        return static_cast<SyntaxKind>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    struct Syntax {
        SyntaxKind kind;
        std::size_t start;
        std::size_t end;

        Syntax(SyntaxKind kind, unsigned int start, unsigned int _end):
            kind(kind),
            start(start),
            end(_end)
        { }

        virtual void accept(class GraphQlSyntaxVisitor*) const = 0;
    };

    struct Name : Syntax {
        Glib::ustring description;
        Glib::ustring identifier;

        C(Name, Glib::ustring identifier),
            I(identifier) { }

        new_operator(Name)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Declaration : Syntax {
        Name* name;
        Glib::ustring description;

        S(Declaration)
        { }
    };

    struct InputType : Declaration {
        std::map<Glib::ustring, Directive*> directives;

        D(InputType, Declaration)
        { }
    };

    struct InputValueDefinition : Declaration {
        Type* type;
        Syntax* default_value;
        std::map<Glib::ustring, Directive*> directives;

        D(InputValueDefinition, Declaration)
        { }

        new_operator(InputValueDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct ArgumentsDefinition : Syntax {
        std::vector<InputValueDefinition*> input_value_definitions;

        S(ArgumentsDefinition)
        { }

        new_operator(ArgumentsDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FieldDefinition : Declaration {
        Glib::ustring description;
        Type* type;
        std::map<Glib::ustring, InputValueDefinition*> arguments;
        std::map<Glib::ustring, Directive*> directives;

        D(FieldDefinition, Declaration)
        { }

        new_operator(FieldDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct InputFieldDefinition : InputType {
        Type* type;

        D(InputFieldDefinition, InputType)
        { }

        new_operator(InputFieldDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct InputFieldsDefinition : Syntax {
        std::vector<InputFieldDefinition*> input_fields_definitions;

        S(InputFieldsDefinition)
        { }

        new_operator(InputFieldsDefinition)

        void accept(GraphQlSyntaxVisitor*) const override;
    };

    struct Implementations : Syntax {
        std::map<Glib::ustring, Name*> implementations;

        S(Implementations)
        { }

        new_operator(Implementations)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct ObjectLike : Declaration {
        std::map<Glib::ustring, FieldDefinition*>* fields;
        std::map<Glib::ustring, Directive*> directives;

        D(ObjectLike, Declaration)
        { }
    };

    struct Object : ObjectLike {
        Implementations* implementations;

        Object(SyntaxKind kind, unsigned int start, unsigned int end) noexcept:
            ObjectLike(kind, start, end)
        { }

        new_operator(Object)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct InputObject : InputType {
        std::map<Glib::ustring, InputFieldDefinition*>* fields;
        std::vector<Glib::ustring> required_fields;

        D(InputObject, InputType)
        { }

        new_operator(InputObject)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Interface : ObjectLike {
        Interface(SyntaxKind kind, unsigned int start, unsigned int end) noexcept:
            ObjectLike(kind, start, end)
        { }

        new_operator(Interface)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct UnionTypeDefinition : Declaration {
        std::map<Glib::ustring, Name*> members;
        std::map<Glib::ustring, Directive*> directives;

        D(UnionTypeDefinition, Declaration)
        { }

        new_operator(UnionTypeDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    enum class DirectiveLocation {
        Unknown,
        EndOfDocument,

        EXECUTABLE_DIRECTIVE_LOCATION_START,
        QUERY,
        MUTATION,
        SUBSCRIPTION,
        FIELD,
        FRAGMENT_DEFINITION,
        FRAGMENT_SPREAD,
        INLINE_FRAGMENT,
        EXECUTABLE_DIRECTIVE_LOCATION_START_END,

        TYPE_SYSTEM_DIRECTIVE_LOCATION_START,
        SCHEMA,
        SCALAR,
        OBJECT,
        FIELD_DEFINITION,
        ARGUMENT_DEFINITION,
        INTERFACE,
        UNION,
        ENUM,
        ENUM_VALUE,
        INPUT_OBJECT,
        INPUT_FIELD_DEFINITION,
        TYPE_SYSTEM_DIRECTIVE_LOCATION_END,
    };

    const std::map<const DirectiveLocation, std::string> directiveLocationToString = {
        { DirectiveLocation::QUERY, "QUERY" },
        { DirectiveLocation::MUTATION, "MUTATION" },
        { DirectiveLocation::SUBSCRIPTION, "SUBSCRIPTION" },
        { DirectiveLocation::FIELD, "FIELD" },
        { DirectiveLocation::FRAGMENT_DEFINITION, "FRAGMENT_DEFINITION" },
        { DirectiveLocation::FRAGMENT_SPREAD, "FRAGMENT_SPREAD" },
        { DirectiveLocation::INLINE_FRAGMENT, "INLINE_FRAGMENT" },

        { DirectiveLocation::SCHEMA, "SCHEMA" },
        { DirectiveLocation::SCALAR, "SCALAR" },
        { DirectiveLocation::OBJECT, "OBJECT" },
        { DirectiveLocation::FIELD_DEFINITION, "FIELD_DEFINITION" },
        { DirectiveLocation::ARGUMENT_DEFINITION, "ARGUMENT_DEFINITION" },
        { DirectiveLocation::INTERFACE, "INTERFACE" },
        { DirectiveLocation::UNION, "UNION" },
        { DirectiveLocation::ENUM, "ENUM" },
        { DirectiveLocation::ENUM_VALUE, "ENUM_VALUE" },
        { DirectiveLocation::INPUT_OBJECT, "INPUT_OBJECT" },
        { DirectiveLocation::INPUT_FIELD_DEFINITION, "INPUT_FIELD_DEFINITION" },
    };

    struct DirectiveDefinition : Declaration {
        std::map<Glib::ustring, InputValueDefinition*> arguments;
        std::set<DirectiveLocation> locations;

        D(DirectiveDefinition, Declaration)
        { }

        new_operator(DirectiveDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct EnumValueDefinition : Name {
        std::map<Glib::ustring, Directive*> directives;

        EnumValueDefinition(SyntaxKind kind, unsigned int start, unsigned int end, Glib::ustring identifier) noexcept:
            Name(kind, start, end, identifier)
        { }

        new_operator(EnumValueDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct EnumTypeDefinition : InputType {
        std::map<Glib::ustring, EnumValueDefinition*> values;

        D(EnumTypeDefinition, InputType)
        { }

        new_operator(EnumTypeDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    enum class TypeEnum : unsigned int {
        T_None,
        T_Int = 1 << 1,
        T_String = 1 << 2,
        T_Float = 1 << 3,
        T_Boolean = 1 << 4,
        T_ID = 1 << 5,

        T_Enum = 1 << 6,
        T_Object = 1 << 7,
        T_Interface = 1 << 8,
        T_Union = 1 << 9,

        T_ScalarType =
            static_cast<unsigned int>(T_Int) |
            static_cast<unsigned int>(T_String) |
            static_cast<unsigned int>(T_Float) |
            static_cast<unsigned int>(T_Boolean) |
            static_cast<unsigned int>(T_ID),

        T_ObjectType =
            static_cast<unsigned int>(T_Object) |
            static_cast<unsigned int>(T_Union) |
            static_cast<unsigned int>(T_Interface),

        T_SymbolicType =
            static_cast<unsigned int>(T_Enum) |
            static_cast<unsigned int>(T_Object),
    };

    inline constexpr TypeEnum operator|(TypeEnum a, TypeEnum b) {
        return static_cast<TypeEnum>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    inline constexpr TypeEnum operator&(TypeEnum a, TypeEnum b) {
        return static_cast<TypeEnum>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    struct Type : Syntax {
        TypeEnum type;
        bool is_non_null;
        bool is_list_type;
        bool is_non_null_list;
        bool in_input_location;
        Name* name;
        DirectiveDefinition* parent_directive_definition;

        S(Type)
        { }

        C(Type, TypeEnum type, bool is_non_null, bool is_list_type, bool is_non_null_list, bool is_input),
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

    struct Value : Syntax {
        S(Value)
        { }

        new_operator(Value)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct NullValue : Value {
        D(NullValue, Value)
        { }

        new_operator(NullValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FloatValue : Value {
        double value;

        D(FloatValue, Value)
        { }

        new_operator(FloatValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct IntValue : Value {
        int value;

        D(IntValue, Value)
        { }

        new_operator(IntValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct StringValue : Value {
        Glib::ustring value;

        D(StringValue, Value)
        { }

        new_operator(StringValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct BooleanValue : Value {
        bool value;

        D(BooleanValue, Value)
        { }

        new_operator(BooleanValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct EnumValue : Value {
        Glib::ustring value;

        D(EnumValue, Value)
        { }

        new_operator(EnumValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct ObjectField : Syntax {
        Name* name;
        Value* value;

        S(ObjectField)
        { }

        new_operator(ObjectField)

        void accept(GraphQlSyntaxVisitor*) const;

    };

    struct ObjectValue : Value {
        std::vector<ObjectField*> object_fields;

        D(ObjectValue, Value)
        { }

        new_operator(ObjectValue)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct ListValue : Value {
        std::vector<Value*> values;

        D(ListValue, Value)
        { }

        new_operator(ListValue)

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
        std::map<Glib::ustring, Directive*> directives;

        S(Argument)
        { }

        new_operator(Argument)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Arguments : Syntax {
        std::vector<Argument*> arguments;

        S(Arguments)
        { }

        new_operator(Arguments)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Directive : Syntax {
        Name* name;
        Arguments* arguments;
        DirectiveDefinition* parent_directive_definition;
        DirectiveLocation location;

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

    struct Selection : Syntax {
        SelectionSet* selection_set;

        S(Selection)
        { }
    };

    struct Field : Selection {
        Name* alias;
        Name* name;
        Arguments* arguments;
        std::map<Glib::ustring, Directive*> directives;

        D(Field, Selection)
        { }

        new_operator(Field)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FragmentSpread : Selection {
        Name* name;
        std::map<Glib::ustring, Directive*> directives;

        D(FragmentSpread, Selection)
        { }

        new_operator(FragmentSpread)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct InlineFragment : Selection {
        std::map<Glib::ustring, Directive*> directives;
        D(InlineFragment, Selection)
        { }

        new_operator(InlineFragment)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct SelectionSet : Syntax {
        std::vector<Selection*> selections;

        S(SelectionSet)
        { }

        new_operator(SelectionSet)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct FragmentDefinition : Syntax {
        Name* name;
        Name* type;
        std::map<Glib::ustring, Directive*> directives;
        SelectionSet* selection_set;

        S(FragmentDefinition)
        { }

        new_operator(FragmentDefinition)

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
        std::map<Glib::ustring, Directive*> directives;

        S(OperationDefinition)
        { }

        new_operator(OperationDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct ExecutableDefinition : Syntax {
        std::vector<OperationDefinition*> operation_definitions;
        std::vector<FragmentDefinition*> fragment_definitions;
        std::vector<DiagnosticMessage> diagnostics;
        const Glib::ustring* source;

        S(ExecutableDefinition)
        { }

        new_operator(ExecutableDefinition)

        void accept(GraphQlSyntaxVisitor*) const;
    };

    struct Schema : Syntax {
        std::map<Glib::ustring, Directive*> directives;
        Name* query;
        Name* mutation;
        Name* subscription;

        Object* query_object;
        Object* mutation_object;
        Object* subscription_object;

        Name* query_key;
        Name* mutation_key;
        Name* subscription_key;

        S(Schema)
        { }

        new_operator(Schema)

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

    enum class SymbolKind : unsigned int {
        SL_None,
        SL_InputObject = 1 << 1,

        SL_Object = 1 << 2,
        SL_Interface = 1 << 3,
        SL_Enum = 1 << 4,
        SL_Union = 1 << 5,

        Input =
            static_cast<unsigned int>(SL_Enum) |
            static_cast<unsigned int>(SL_InputObject),

        Output =
            static_cast<unsigned int>(SL_Object) |
            static_cast<unsigned int>(SL_Interface) |
            static_cast<unsigned int>(SL_Enum) |
            static_cast<unsigned int>(SL_Union),
    };

    inline constexpr SymbolKind operator|(SymbolKind a, SymbolKind b) {
        return static_cast<SymbolKind>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    inline constexpr SymbolKind operator&(SymbolKind a, SymbolKind b) {
        return static_cast<SymbolKind>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    struct Symbol {
        Glib::ustring* name;
        Declaration* declaration;
        SymbolKind kind;

        Symbol(Glib::ustring* name, Declaration* declaration, SymbolKind kind):
            name(name),
            declaration(declaration),
            kind(kind)
        { }

        new_operator(Symbol)
    };

    class GraphQlSyntaxVisitor {
    public:
        virtual void visit(const Argument*) = 0;
        virtual void visit(const Arguments*) = 0;
        virtual void visit(const ArgumentsDefinition*) = 0;
        virtual void visit(const InputValueDefinition*) = 0;
        virtual void visit(const Name*) = 0;
        virtual void visit(const Value*) = 0;
        virtual void visit(const Type*) = 0;
        virtual void visit(const IntValue*) = 0;
        virtual void visit(const NullValue*) = 0;
        virtual void visit(const FloatValue*) = 0;
        virtual void visit(const StringValue*) = 0;
        virtual void visit(const BooleanValue*) = 0;
        virtual void visit(const EnumValue*) = 0;
        virtual void visit(const ObjectField*) = 0;
        virtual void visit(const ObjectValue*) = 0;
        virtual void visit(const ListValue*) = 0;
        virtual void visit(const VariableDefinition*) = 0;
        virtual void visit(const VariableDefinitions*) = 0;
        virtual void visit(const Field*) = 0;
        virtual void visit(const SelectionSet*) = 0;
        virtual void visit(const Selection*) = 0;
        virtual void visit(const OperationDefinition*) = 0;
        virtual void visit(const FragmentDefinition*) = 0;
        virtual void visit(const ExecutableDefinition*) = 0;

        virtual void visit(const SchemaDocument*) = 0;
        virtual void visit(const Implementations*) = 0;
        virtual void visit(const Object*) = 0;
        virtual void visit(const InputObject*) = 0;
        virtual void visit(const Interface*) = 0;
        virtual void visit(const Directive*) = 0;
        virtual void visit(const DirectiveDefinition*) = 0;
        virtual void visit(const EnumTypeDefinition*) = 0;
        virtual void visit(const EnumValueDefinition*) = 0;
        virtual void visit(const FieldDefinition*) = 0;
        virtual void visit(const InputFieldDefinition*) = 0;
        virtual void visit(const InputFieldsDefinition*) = 0;
        virtual void visit(const UnionTypeDefinition*) = 0;
        virtual void visit(const FragmentSpread*) = 0;
        virtual void visit(const Schema*) = 0;
    };
};

#endif //FLASHPOINT_GRAPHQL_SYNTAXES_H

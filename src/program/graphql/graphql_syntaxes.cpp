#include "graphql_syntaxes.h"

namespace flashpoint::program::graphql {

    void Argument::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Arguments::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void ArgumentsDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void InputValueDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void InputFieldDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void InputFieldsDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void InputObject::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Directive::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void DirectiveDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void EnumTypeDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void FragmentSpread::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Name::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Value::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Type::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void IntValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void NullValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void FloatValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void EnumValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void EnumValueDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void StringValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void BooleanValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void ObjectField::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void ObjectValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void ListValue::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void VariableDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void VariableDefinitions::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Field::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void InlineFragment::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void SelectionSet::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void OperationDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void FragmentDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void ExecutableDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void SchemaDocument::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Object::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Interface::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Implementations::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void FieldDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void UnionTypeDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Schema::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }
};
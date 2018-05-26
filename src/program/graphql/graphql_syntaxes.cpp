
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

    void IntLiteral::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void NullLiteral::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void FloatLiteral::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void StringLiteral::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void BooleanLiteral::accept(GraphQlSyntaxVisitor* visitor) const
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

    void QueryField::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void SelectionSet::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }

    void Selection::accept(GraphQlSyntaxVisitor* visitor) const
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

    void QueryDocument::accept(GraphQlSyntaxVisitor* visitor) const
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

    void FieldsDefinition::accept(GraphQlSyntaxVisitor* visitor) const
    {
        visitor->visit(this);
    }
};
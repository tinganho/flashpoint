// This code is auto generate. Don't edit it!
#ifndef graphql_diagnostics_H
#define graphql_diagnostics_H

#include <program/diagnostic.h>

using namespace flashpoint::program;

namespace flashpoint::program {

class D {
public:
    static DiagnosticMessageTemplate A_schema_definition_can_only_contain_query_mutation_and_subscription_fields;
    static DiagnosticMessageTemplate Cannot_add_an_output_type_to_an_input_location;
    static DiagnosticMessageTemplate Cannot_add_an_input_type_to_an_output_location;
    static DiagnosticMessageTemplate Cannot_annotate_with_a_type_that_recursively_references_the_directive_0;
    static DiagnosticMessageTemplate Directive_cannot_reference_itself;
    static DiagnosticMessageTemplate Duplicate_argument_0;
    static DiagnosticMessageTemplate Duplicate_field_0;
    static DiagnosticMessageTemplate Duplicate_fragment_0;
    static DiagnosticMessageTemplate Duplicate_type_0;
    static DiagnosticMessageTemplate Expected_0_but_got_1;
    static DiagnosticMessageTemplate Expected_0_but_instead_reached_the_end_of_document;
    static DiagnosticMessageTemplate Expected_at_least_a_field_inline_fragment_or_fragment_spread;
    static DiagnosticMessageTemplate Expected_directive_location_but_instead_reached_the_end_of_document;
    static DiagnosticMessageTemplate Expected_directive_location;
    static DiagnosticMessageTemplate Expected_enum_value_but_instead_got_0;
    static DiagnosticMessageTemplate Expected_field_definition;
    static DiagnosticMessageTemplate Expected_field_inline_fragment_or_fragment_spread;
    static DiagnosticMessageTemplate Expected_fragment_spread_or_inline_fragment;
    static DiagnosticMessageTemplate Expected_name_of_argument;
    static DiagnosticMessageTemplate Expected_name_of_interface;
    static DiagnosticMessageTemplate Expected_name_of_object;
    static DiagnosticMessageTemplate Expected_name_but_instead_got_0;
    static DiagnosticMessageTemplate Expected_object_field_name;
    static DiagnosticMessageTemplate Expected_operation_definition_or_fragment_definition;
    static DiagnosticMessageTemplate Expected_type_annotation_instead_got_0;
    static DiagnosticMessageTemplate Expected_type_definition;
    static DiagnosticMessageTemplate Expected_type_name_instead_got_0;
    static DiagnosticMessageTemplate Expected_value_instead_got_0;
    static DiagnosticMessageTemplate Field_0_does_not_exist_on_type_1;
    static DiagnosticMessageTemplate Fragment_0_is_not_defined;
    static DiagnosticMessageTemplate Interface_0_is_not_defined;
    static DiagnosticMessageTemplate Invalid_Unicode_escape_sequence;
    static DiagnosticMessageTemplate Missing_required_field_0;
    static DiagnosticMessageTemplate Mutations_are_not_supported;
    static DiagnosticMessageTemplate No_query_root_operation_type_defined;
    static DiagnosticMessageTemplate Only_objects_are_allowed_as_members_in_a_union;
    static DiagnosticMessageTemplate Subscriptions_are_not_supported;
    static DiagnosticMessageTemplate The_directive_0_does_not_support_the_location_1;
    static DiagnosticMessageTemplate The_field_0_in_interface_1_is_not_implemented;
    static DiagnosticMessageTemplate The_object_0_does_not_implement_the_interface_1;
    static DiagnosticMessageTemplate The_object_0_does_not_match_the_object_1;
    static DiagnosticMessageTemplate The_type_0_is_not_an_object;
    static DiagnosticMessageTemplate The_type_0_is_not_member_of_the_union_1;
    static DiagnosticMessageTemplate The_value_0_is_not_assignable_to_type_1;
    static DiagnosticMessageTemplate Type_0_does_not_match_type_1_from_the_interface_2;
    static DiagnosticMessageTemplate Type_0_is_not_assignable_to_type_1;
    static DiagnosticMessageTemplate Type_0_is_not_defined;
    static DiagnosticMessageTemplate Type_conditions_are_only_applicable_inside_object_interfaces_and_unions;
    static DiagnosticMessageTemplate Undefined_directive_0;
    static DiagnosticMessageTemplate Undefined_enum_value_0;
    static DiagnosticMessageTemplate Unexpected_end_of_implementations;
    static DiagnosticMessageTemplate Unknown_argument_0;
    static DiagnosticMessageTemplate Unterminated_string_value;
};

}

#endif // graphql_diagnostics_H
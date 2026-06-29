#pragma once

#include <cpporm/annotations.hpp>
#include <cpporm/config.hpp>
#include <cpporm/descriptors.hpp>
#include <cpporm/fixed_string.hpp>
#include <cpporm/relations.hpp>

#include <cstddef>
#include <meta>

namespace cpporm {

consteval auto has_annotation(std::meta::info item, std::meta::info annotation_type) -> bool {
    return !std::meta::annotations_of_with_type(item, annotation_type).empty();
}

consteval auto is_primary_key_field(std::meta::info field) -> bool {
    return has_annotation(field, ^^primary_key) || has_annotation(field, ^^id);
}

consteval auto is_unique_field(std::meta::info field) -> bool {
    return has_annotation(field, ^^unique);
}

consteval auto is_model_constraint_member(std::meta::info member) -> bool {
    return (std::meta::is_variable(member) || std::meta::is_nonstatic_data_member(member))
        && std::meta::dealias(std::meta::remove_cvref(std::meta::type_of(member))) == ^^model_constraint;
}

consteval auto field_id_annotation_is_valid(std::meta::info field) -> bool {
    auto annotations = std::meta::annotations_of_with_type(field, ^^id);
    return annotations.empty() || std::meta::extract<id>(annotations.front()).field_count == 0;
}

consteval auto field_unique_annotation_is_valid(std::meta::info field) -> bool {
    auto annotations = std::meta::annotations_of_with_type(field, ^^unique);
    return annotations.empty() || std::meta::extract<unique>(annotations.front()).field_count == 0;
}

consteval auto is_direct_struct_model(std::meta::info item) -> bool {
    return std::meta::is_type(item)
        && std::meta::is_class_type(item)
        && std::meta::is_complete_type(item)
        && !std::meta::is_union_type(item);
}

consteval auto table_name_of(std::meta::info model) -> fixed_string {
    auto annotations = std::meta::annotations_of_with_type(model, ^^table);
    if (annotations.empty()) {
        return fixed_string{std::meta::identifier_of(model)};
    }

    return std::meta::extract<table>(annotations.front()).name;
}

consteval auto column_name_of(std::meta::info field) -> fixed_string {
    auto annotations = std::meta::annotations_of_with_type(field, ^^column);
    if (annotations.empty()) {
        return fixed_string{std::meta::identifier_of(field)};
    }

    return std::meta::extract<column>(annotations.front()).name;
}

consteval auto referenced_field_of(std::meta::info field) -> std::meta::info {
    std::meta::info referenced_field{};
    for (auto annotation : std::meta::annotations_of(field)) {
        auto annotation_type = std::meta::type_of(annotation);
        if (!std::meta::has_template_arguments(annotation_type)
            || std::meta::template_of(annotation_type) != ^^references) {
            continue;
        }

        if (referenced_field != std::meta::info{}) {
            throw "cpporm field must not have multiple references annotations";
        }

        auto target_field = std::meta::template_arguments_of(annotation_type).front();
        auto target_field_type = std::meta::remove_cvref(std::meta::type_of(target_field));
        if (!std::meta::has_template_arguments(target_field_type)
            || std::meta::template_of(target_field_type) != ^^field_ref) {
            throw "cpporm references annotation must target a cpporm field accessor";
        }

        referenced_field = std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(target_field_type).front()
        );
    }

    return referenced_field;
}

consteval auto is_relation_field(std::meta::info field) -> bool {
    auto type = std::meta::dealias(std::meta::remove_cvref(std::meta::type_of(field)));
    if (!std::meta::has_template_arguments(type)) {
        return false;
    }

    auto template_info = std::meta::template_of(type);
    return template_info == ^^relation || template_info == ^^has_many;
}

consteval auto relation_kind_of(std::meta::info field) -> relation_kind {
    auto type = std::meta::dealias(std::meta::remove_cvref(std::meta::type_of(field)));
    auto template_info = std::meta::template_of(type);
    if (template_info == ^^relation) {
        return relation_kind::relation;
    }

    if (template_info == ^^has_many) {
        return relation_kind::has_many;
    }

    throw "cpporm field is not a relation";
}

consteval auto relation_target_model_of(std::meta::info field) -> std::meta::info {
    auto type = std::meta::dealias(std::meta::remove_cvref(std::meta::type_of(field)));
    return std::meta::template_arguments_of(type).front();
}

consteval auto namespace_registers_model(std::meta::info namespace_info, std::meta::info model) -> bool {
    auto members = std::meta::members_of(
        namespace_info,
        std::meta::access_context::unchecked()
    );

    for (auto member : members) {
        if (is_direct_struct_model(member) && member == model) {
            return true;
        }
    }

    return false;
}

consteval auto reference_field_to(std::meta::info source_model, std::meta::info target_model) -> std::meta::info {
    std::meta::info local_field{};
    for (auto field : std::meta::nonstatic_data_members_of(
             source_model,
             std::meta::access_context::unchecked())) {
        auto target_field = referenced_field_of(field);
        if (target_field == std::meta::info{}) {
            continue;
        }

        if (std::meta::parent_of(target_field) != target_model) {
            continue;
        }

        if (local_field != std::meta::info{}) {
            throw "cpporm relation target is ambiguous";
        }

        local_field = field;
    }

    return local_field;
}

consteval auto inverse_reference_field_to(std::meta::info namespace_info, std::meta::info source_model, std::meta::info target_model) -> std::meta::info {
    if (!namespace_registers_model(namespace_info, target_model)) {
        throw "cpporm relation target model must be registered in the same namespace";
    }

    auto local_field = reference_field_to(target_model, source_model);
    if (local_field == std::meta::info{}) {
        throw "cpporm has_many target has no reference back to source model";
    }

    return local_field;
}

consteval auto make_relation_descriptor(std::meta::info source_model, std::meta::info relation_field, std::meta::info target_model) -> relation_descriptor {
    auto local_field = reference_field_to(source_model, target_model);
    if (local_field == std::meta::info{}) {
        throw "cpporm relation target has no reference from source model";
    }

    auto target_field = referenced_field_of(local_field);
    return relation_descriptor{
        .member_name = fixed_string{std::meta::identifier_of(relation_field)},
        .kind = relation_kind::relation,
        .target_model_name = fixed_string{std::meta::identifier_of(target_model)},
        .target_table_name = table_name_of(target_model),
        .local_member_name = fixed_string{std::meta::identifier_of(local_field)},
        .local_column_name = column_name_of(local_field),
        .target_member_name = fixed_string{std::meta::identifier_of(target_field)},
        .target_column_name = column_name_of(target_field)
    };
}

consteval auto make_has_many_descriptor(std::meta::info namespace_info, std::meta::info source_model, std::meta::info relation_field, std::meta::info target_model) -> relation_descriptor {
    auto target_local_field = inverse_reference_field_to(namespace_info, source_model, target_model);
    auto source_field = referenced_field_of(target_local_field);
    return relation_descriptor{
        .member_name = fixed_string{std::meta::identifier_of(relation_field)},
        .kind = relation_kind::has_many,
        .target_model_name = fixed_string{std::meta::identifier_of(target_model)},
        .target_table_name = table_name_of(target_model),
        .local_member_name = fixed_string{std::meta::identifier_of(target_local_field)},
        .local_column_name = column_name_of(target_local_field),
        .target_member_name = fixed_string{std::meta::identifier_of(source_field)},
        .target_column_name = column_name_of(source_field)
    };
}

consteval auto describe_model(std::meta::info namespace_info, std::meta::info model) -> model_descriptor {
    model_descriptor descriptor{
        .type_name = fixed_string{std::meta::identifier_of(model)},
        .table_name = table_name_of(model),
        .field_count = 0,
        .fields = {},
        .relation_count = 0,
        .relations = {}
    };

    auto fields = std::meta::nonstatic_data_members_of(
        model,
        std::meta::access_context::unchecked()
    );

    for (auto field : fields) {
        if (is_relation_field(field)) {
            continue;
        }

        if (descriptor.field_count >= max_model_fields) {
            throw "cpporm model has too many fields";
        }

        auto referenced_field = referenced_field_of(field);
        auto referenced_model = referenced_field == std::meta::info{} ? std::meta::info{} : std::meta::parent_of(referenced_field);

        descriptor.fields[descriptor.field_count] = field_descriptor{
            .member_name = fixed_string{std::meta::identifier_of(field)},
            .column_name = column_name_of(field),
            .primary_key = is_primary_key_field(field),
            .unique = is_unique_field(field),
            .ignored = has_annotation(field, ^^ignore),
            .has_reference = referenced_field != std::meta::info{},
            .referenced_model_name = referenced_model == std::meta::info{} ? fixed_string{} : fixed_string{std::meta::identifier_of(referenced_model)},
            .referenced_table_name = referenced_model == std::meta::info{} ? fixed_string{} : table_name_of(referenced_model),
            .referenced_member_name = referenced_field == std::meta::info{} ? fixed_string{} : fixed_string{std::meta::identifier_of(referenced_field)},
            .referenced_column_name = referenced_field == std::meta::info{} ? fixed_string{} : column_name_of(referenced_field)
        };
        ++descriptor.field_count;
    }

    for (auto field : fields) {
        if (!is_relation_field(field)) {
            continue;
        }

        if (descriptor.relation_count >= max_model_relations) {
            throw "cpporm model has too many relations";
        }

        auto target_model = relation_target_model_of(field);
        if (!namespace_registers_model(namespace_info, target_model)) {
            throw "cpporm relation target model must be registered in the same namespace";
        }

        if (relation_kind_of(field) == relation_kind::relation) {
            descriptor.relations[descriptor.relation_count] = make_relation_descriptor(model, field, target_model);
        } else {
            descriptor.relations[descriptor.relation_count] = make_has_many_descriptor(namespace_info, model, field, target_model);
        }
        ++descriptor.relation_count;
    }

    return descriptor;
}

consteval auto validate_reference(std::meta::info namespace_info, std::meta::info field) -> void {
    auto referenced_field = referenced_field_of(field);
    if (referenced_field == std::meta::info{}) {
        return;
    }

    if (!std::meta::has_parent(referenced_field)) {
        throw "cpporm referenced field must belong to a model";
    }

    auto referenced_model = std::meta::parent_of(referenced_field);
    if (!namespace_registers_model(namespace_info, referenced_model)) {
        throw "cpporm referenced model must be registered in the same namespace";
    }

    if (has_annotation(referenced_field, ^^ignore)) {
        throw "cpporm referenced field must not be ignored";
    }

    if (std::meta::type_of(field) != std::meta::type_of(referenced_field)) {
        throw "cpporm reference field type must match referenced field type";
    }
}

consteval auto validate_model_references(std::meta::info namespace_info, std::meta::info model) -> void {
    for (auto field : std::meta::nonstatic_data_members_of(
             model,
             std::meta::access_context::unchecked())) {
        if (!field_id_annotation_is_valid(field)) {
            throw "cpporm field id annotation must not list fields";
        }

        if (!field_unique_annotation_is_valid(field)) {
            throw "cpporm field unique annotation must not list fields";
        }

        validate_reference(namespace_info, field);
    }
}

consteval auto model_field_named(std::meta::info model, fixed_string const& name) -> std::meta::info {
    std::meta::info result{};
    for (auto field : std::meta::nonstatic_data_members_of(
             model,
             std::meta::access_context::unchecked())) {
        if (!std::meta::has_identifier(field) || std::meta::identifier_of(field) != name.view()) {
            continue;
        }

        if (result != std::meta::info{}) {
            throw "cpporm constraint field is ambiguous";
        }

        result = field;
    }

    if (result == std::meta::info{}) {
        throw "cpporm constraint references unknown field";
    }

    if (has_annotation(result, ^^ignore) || is_relation_field(result)) {
        throw "cpporm constraint field must be persisted scalar field";
    }

    return result;
}

consteval auto validate_constraint_fields(std::meta::info model, constraint_fields const& fields) -> void {
    if (fields.field_count < 2) {
        throw "cpporm composite constraint requires at least two fields";
    }

    for (std::size_t index = 0; index < fields.field_count; ++index) {
        (void)model_field_named(model, fields.fields[index]);
        for (std::size_t right = index + 1; right < fields.field_count; ++right) {
            if (fields.fields[index].view() == fields.fields[right].view()) {
                throw "cpporm composite constraint must not repeat fields";
            }
        }
    }
}

consteval auto validate_model_constraints(std::meta::info model) -> void {
    for (auto member : std::meta::members_of(model, std::meta::access_context::unchecked())) {
        if (!is_model_constraint_member(member)) {
            continue;
        }

        if (std::meta::is_nonstatic_data_member(member)) {
            throw "cpporm model_constraint must be static constexpr";
        }

        auto id_annotations = std::meta::annotations_of_with_type(member, ^^id);
        auto unique_annotations = std::meta::annotations_of_with_type(member, ^^unique);
        if (!id_annotations.empty() && !unique_annotations.empty()) {
            throw "cpporm model_constraint must have exactly one constraint annotation";
        }

        if (!id_annotations.empty()) {
            validate_constraint_fields(model, std::meta::extract<id>(id_annotations.front()));
        }

        if (!unique_annotations.empty()) {
            validate_constraint_fields(model, std::meta::extract<unique>(unique_annotations.front()));
        }
    }
}

consteval auto register_namespace(std::meta::info namespace_info) -> registry_descriptor {
    registry_descriptor registry{};
    auto members = std::meta::members_of(
        namespace_info,
        std::meta::access_context::unchecked()
    );

    for (auto member : members) {
        if (!is_direct_struct_model(member)) {
            continue;
        }

        if (registry.model_count >= max_registered_models) {
            throw "cpporm namespace has too many registered models";
        }

        validate_model_references(namespace_info, member);
        validate_model_constraints(member);
        registry.models[registry.model_count] = describe_model(namespace_info, member);
        ++registry.model_count;
    }

    return registry;
}

} // namespace cpporm

#pragma once

#include <cpporm/dialects/sqlite.hpp>
#include <cpporm/registry.hpp>
#include <cpporm/sql_string.hpp>

#include <meta>
#include <string_view>

namespace cpporm {

template<std::meta::info Field>
consteval auto is_persisted_field() -> bool {
    return !has_annotation(Field, ^^ignore) && !is_relation_field(Field);
}

template<std::meta::info Model>
consteval auto primary_key_field_of() -> std::meta::info {
    std::meta::info primary_key_field{};
    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    template for (constexpr std::meta::info field : fields) {
        if constexpr (!is_persisted_field<field>()) {
            continue;
        }

        if constexpr (is_primary_key_field(field)) {
            if (primary_key_field != std::meta::info{}) {
                throw "cpporm mutation requires exactly one primary key";
            }
            primary_key_field = field;
        }
    }

    if (primary_key_field == std::meta::info{}) {
        throw "cpporm mutation requires exactly one primary key";
    }

    return primary_key_field;
}

template<std::meta::info Model>
consteval auto persisted_field_count() -> std::size_t {
    std::size_t count = 0;
    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    template for (constexpr std::meta::info field : fields) {
        if constexpr (is_persisted_field<field>()) {
            ++count;
        }
    }

    return count;
}

template<std::meta::info Model>
consteval auto non_primary_persisted_field_count() -> std::size_t {
    constexpr auto primary_key = primary_key_field_of<Model>();
    std::size_t count = 0;
    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    template for (constexpr std::meta::info field : fields) {
        if constexpr (is_persisted_field<field>() && field != primary_key) {
            ++count;
        }
    }

    return count;
}

template<std::meta::info Payload>
consteval auto payload_member_named(std::string_view name) -> std::meta::info {
    if (!std::meta::is_type(Payload) || !std::meta::is_class_type(Payload) || !std::meta::is_complete_type(Payload)) {
        throw "cpporm mutation payload must be a complete class type";
    }

    for (auto member : std::meta::nonstatic_data_members_of(
             Payload,
             std::meta::access_context::unchecked())) {
        if (std::meta::has_identifier(member) && std::meta::identifier_of(member) == name) {
            return member;
        }
    }

    return {};
}

template<std::meta::info ModelField, std::meta::info Payload>
consteval auto payload_field_for() -> std::meta::info {
    return payload_member_named<Payload>(std::meta::identifier_of(ModelField));
}

template<std::meta::info Model, std::meta::info Payload>
consteval auto insert_payload_field_count() -> std::size_t {
    std::size_t count = 0;
    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    template for (constexpr std::meta::info field : fields) {
        if constexpr (is_persisted_field<field>() && payload_field_for<field, Payload>() != std::meta::info{}) {
            ++count;
        }
    }

    return count;
}

template<std::meta::info Model, std::meta::info Payload>
consteval auto update_payload_field_count() -> std::size_t {
    constexpr auto primary_key = primary_key_field_of<Model>();
    std::size_t count = 0;
    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    template for (constexpr std::meta::info field : fields) {
        if constexpr (is_persisted_field<field>() && field != primary_key && payload_field_for<field, Payload>() != std::meta::info{}) {
            ++count;
        }
    }

    return count;
}

template<std::meta::info Model, std::meta::info Payload>
consteval auto payload_has_primary_key() -> bool {
    constexpr auto primary_key = primary_key_field_of<Model>();
    return payload_field_for<primary_key, Payload>() != std::meta::info{};
}

template<class Dialect, std::meta::info Model, std::meta::info Payload>
consteval auto make_insert_sql() -> sql_string {
    if constexpr (insert_payload_field_count<Model, Payload>() == 0) {
        throw "cpporm insert payload must contain at least one model field";
    }

    sql_string sql{};
    sql.append("INSERT INTO ");
    Dialect::append_identifier(sql, table_name_of(Model).view());
    sql.append(" (");

    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    bool first = true;
    template for (constexpr std::meta::info field : fields) {
        if constexpr (!is_persisted_field<field>() || payload_field_for<field, Payload>() == std::meta::info{}) {
            continue;
        }

        if (first) {
            first = false;
        } else {
            sql.append(", ");
        }
        Dialect::append_identifier(sql, column_name_of(field).view());
    }

    sql.append(") VALUES (");
    first = true;
    template for (constexpr std::meta::info field : fields) {
        if constexpr (!is_persisted_field<field>() || payload_field_for<field, Payload>() == std::meta::info{}) {
            continue;
        }

        if (first) {
            first = false;
        } else {
            sql.append(", ");
        }
        Dialect::append_parameter(sql);
    }

    sql.push_back(')');
    return sql;
}

template<class Dialect, std::meta::info Model, std::meta::info Payload>
consteval auto make_update_sql() -> sql_string {
    constexpr auto primary_key = primary_key_field_of<Model>();
    if constexpr (!payload_has_primary_key<Model, Payload>()) {
        throw "cpporm update payload must contain primary key";
    }

    if constexpr (update_payload_field_count<Model, Payload>() == 0) {
        throw "cpporm update payload must contain at least one non-primary-key model field";
    }

    sql_string sql{};
    sql.append("UPDATE ");
    Dialect::append_identifier(sql, table_name_of(Model).view());
    sql.append(" SET ");

    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    bool first = true;
    template for (constexpr std::meta::info field : fields) {
        if constexpr (!is_persisted_field<field>() || field == primary_key || payload_field_for<field, Payload>() == std::meta::info{}) {
            continue;
        }

        if (first) {
            first = false;
        } else {
            sql.append(", ");
        }
        Dialect::append_identifier(sql, column_name_of(field).view());
        sql.append(" = ");
        Dialect::append_parameter(sql);
    }

    sql.append(" WHERE ");
    Dialect::append_identifier(sql, column_name_of(primary_key).view());
    sql.append(" = ");
    Dialect::append_parameter(sql);
    return sql;
}

template<class Dialect, std::meta::info Model, std::meta::info Payload>
consteval auto make_upsert_sql() -> sql_string {
    constexpr auto primary_key = primary_key_field_of<Model>();
    if constexpr (!payload_has_primary_key<Model, Payload>()) {
        throw "cpporm upsert payload must contain primary key";
    }

    if constexpr (update_payload_field_count<Model, Payload>() == 0) {
        throw "cpporm upsert payload must contain at least one non-primary-key model field";
    }

    auto sql = make_insert_sql<Dialect, Model, Payload>();
    sql.append(" ON CONFLICT (");
    Dialect::append_identifier(sql, column_name_of(primary_key).view());
    sql.append(") DO UPDATE SET ");

    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));

    bool first = true;
    template for (constexpr std::meta::info field : fields) {
        if constexpr (!is_persisted_field<field>() || field == primary_key || payload_field_for<field, Payload>() == std::meta::info{}) {
            continue;
        }

        if (first) {
            first = false;
        } else {
            sql.append(", ");
        }
        Dialect::append_identifier(sql, column_name_of(field).view());
        sql.append(" = excluded.");
        Dialect::append_identifier(sql, column_name_of(field).view());
    }

    return sql;
}

template<class Model, class Values>
consteval auto sqlite::insert_sql() -> sql_string {
    return make_insert_sql<sqlite, ^^Model, ^^Values>();
}

template<class Model, class Values>
consteval auto sqlite::update_sql() -> sql_string {
    return make_update_sql<sqlite, ^^Model, ^^Values>();
}

template<class Model, class Values>
consteval auto sqlite::upsert_sql() -> sql_string {
    return make_upsert_sql<sqlite, ^^Model, ^^Values>();
}

} // namespace cpporm

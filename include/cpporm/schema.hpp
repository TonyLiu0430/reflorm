#pragma once

#include <cpporm/dialects/sqlite.hpp>
#include <cpporm/registry.hpp>
#include <cpporm/sql_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace cpporm {

struct schema_plan {
    std::size_t statement_count = 0;
    std::array<sql_string, max_registered_models> statements{};
};

template<class T>
struct is_optional : std::false_type {};

template<class T>
struct is_optional<std::optional<T>> : std::true_type {};

template<class T>
inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

template<class T>
struct optional_value {
    using type = T;
};

template<class T>
struct optional_value<std::optional<T>> {
    using type = T;
};

template<class T>
using optional_value_t = typename optional_value<std::remove_cvref_t<T>>::type;

template<class T>
concept sqlite_blob_value = std::same_as<std::remove_cvref_t<T>, std::vector<std::byte>>
    || std::same_as<std::remove_cvref_t<T>, std::vector<unsigned char>>;

template<std::meta::info Field>
consteval auto sqlite_type_name_of() -> std::string_view {
    using raw_value_type = typename[:std::meta::dealias(std::meta::type_of(Field)):];
    using value_type = optional_value_t<raw_value_type>;

    if constexpr (std::integral<value_type>) {
        return "INTEGER";
    } else if constexpr (std::floating_point<value_type>) {
        return "REAL";
    } else if constexpr (std::same_as<value_type, std::string>) {
        return "TEXT";
    } else if constexpr (sqlite_blob_value<value_type>) {
        return "BLOB";
    } else {
        throw "cpporm unsupported sqlite schema field type";
    }
}

template<std::meta::info Field>
consteval auto sqlite_field_is_nullable() -> bool {
    using value_type = typename[:std::meta::dealias(std::meta::type_of(Field)):];
    return is_optional_v<value_type>;
}

template<class Dialect, std::meta::info Field>
consteval auto append_column_definition(sql_string& sql) -> void {
    Dialect::append_identifier(sql, column_name_of(Field).view());
    sql.push_back(' ');
    sql.append(sqlite_type_name_of<Field>());

    if (has_annotation(Field, ^^primary_key)) {
        sql.append(" PRIMARY KEY");
    } else if (!sqlite_field_is_nullable<Field>()) {
        sql.append(" NOT NULL");
    }
}

template<class Dialect, std::meta::info Field>
consteval auto append_foreign_key_definition(sql_string& sql) -> void {
    auto referenced_field = referenced_field_of(Field);
    if (referenced_field == std::meta::info{}) {
        return;
    }

    sql.append("FOREIGN KEY (");
    Dialect::append_identifier(sql, column_name_of(Field).view());
    sql.append(") REFERENCES ");
    Dialect::append_identifier(sql, table_name_of(std::meta::parent_of(referenced_field)).view());
    sql.append(" (");
    Dialect::append_identifier(sql, column_name_of(referenced_field).view());
    sql.push_back(')');
}

template<class Dialect, std::meta::info Model, bool IfNotExists>
consteval auto make_create_table_sql() -> sql_string {
    if (!std::meta::is_type(Model) || !std::meta::is_class_type(Model) || !std::meta::is_complete_type(Model)) {
        throw "cpporm create_table model must be a complete class type";
    }

    sql_string sql{};
    sql.append("CREATE TABLE ");
    if constexpr (IfNotExists) {
        sql.append("IF NOT EXISTS ");
    }
    Dialect::append_identifier(sql, table_name_of(Model).view());
    sql.append(" (");

    bool first = true;
    static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    ));
    template for (constexpr std::meta::info field : fields) {
        if (has_annotation(field, ^^ignore) || is_relation_field(field)) {
            continue;
        }

        if (first) {
            first = false;
        } else {
            sql.append(", ");
        }

        append_column_definition<Dialect, field>(sql);
    }

    template for (constexpr std::meta::info field : fields) {
        if (has_annotation(field, ^^ignore) || is_relation_field(field) || referenced_field_of(field) == std::meta::info{}) {
            continue;
        }

        if (first) {
            first = false;
        } else {
            sql.append(", ");
        }

        append_foreign_key_definition<Dialect, field>(sql);
    }

    if (first) {
        throw "cpporm create_table requires at least one column";
    }

    sql.push_back(')');
    return sql;
}

template<class Model>
consteval auto sqlite::create_table() -> sql_string {
    return make_create_table_sql<sqlite, ^^Model, false>();
}

template<class Model>
consteval auto sqlite::create_table_if_not_exists() -> sql_string {
    return make_create_table_sql<sqlite, ^^Model, true>();
}

template<std::meta::info Namespace>
consteval auto sqlite::create_schema() {
    schema_plan plan{};
    static constexpr auto members = std::define_static_array(std::meta::members_of(
        Namespace,
        std::meta::access_context::unchecked()
    ));

    template for (constexpr std::meta::info member : members) {
        if (!is_direct_struct_model(member)) {
            continue;
        }

        if (plan.statement_count >= max_registered_models) {
            throw "cpporm schema has too many models";
        }

        plan.statements[plan.statement_count] = make_create_table_sql<sqlite, member, true>();
        ++plan.statement_count;
    }

    return plan;
}

} // namespace cpporm

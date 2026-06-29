#pragma once

#include <cpporm/sql_string.hpp>

#include <meta>
#include <tuple>
#include <string_view>

namespace cpporm {

struct sqlite;

template<std::meta::info Field>
struct field_ref;

template<class Dialect, std::size_t LimitValue, class Bindings, std::meta::info Model, std::meta::info... Fields>
struct select_query;

template<class Dialect, std::meta::info Model>
struct find_many_builder;

struct sqlite {
    class database;

    static constexpr auto append_identifier(sql_string& sql, std::string_view identifier) -> void {
        sql.push_back('"');
        for (char character : identifier) {
            if (character == '"') {
                sql.push_back('"');
            }
            sql.push_back(character);
        }
        sql.push_back('"');
    }

    static constexpr auto append_parameter(sql_string& sql) -> void {
        sql.push_back('?');
    }

    template<std::meta::info Model, std::meta::info... Fields>
    static consteval auto select() -> select_query<sqlite, dynamic_extent, std::tuple<>, Model, Fields...>;

    template<class Model, std::meta::info... Fields>
    static consteval auto select(field_ref<Fields> const&...) -> select_query<sqlite, dynamic_extent, std::tuple<>, ^^Model, Fields...>;

    template<class Model>
    static consteval auto find_many() -> find_many_builder<sqlite, ^^Model>;

    template<class Model>
    static consteval auto create_table() -> sql_string;

    template<class Model>
    static consteval auto create_table_if_not_exists() -> sql_string;

    template<std::meta::info Namespace>
    static consteval auto create_schema();

    template<class Model, class Values = Model>
    static consteval auto insert_sql() -> sql_string;

    template<class Model, class Values = Model>
    static consteval auto update_sql() -> sql_string;

    template<class Model, class Values = Model>
    static consteval auto upsert_sql() -> sql_string;
};

} // namespace cpporm

#pragma once

#include <cpporm/annotations.hpp>
#include <cpporm/dialects/sqlite.hpp>
#include <cpporm/fixed_string.hpp>
#include <cpporm/materialization.hpp>
#include <cpporm/ordering.hpp>
#include <cpporm/predicate.hpp>
#include <cpporm/registry.hpp>
#include <cpporm/sql_string.hpp>

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cpporm {

template<std::meta::info Model, std::meta::info Field>
consteval auto is_model_field() -> bool {
    auto fields = std::meta::nonstatic_data_members_of(
        Model,
        std::meta::access_context::unchecked()
    );

    for (auto field : fields) {
        if (field == Field) {
            return true;
        }
    }

    return false;
}

template<std::meta::info Field, std::meta::info... Rest>
consteval auto contains_field() -> bool {
    return ((Field == Rest) || ...);
}

template<std::meta::info... Fields>
consteval auto has_duplicate_fields() -> bool {
    if constexpr (sizeof...(Fields) == 0) {
        return false;
    } else {
        constexpr std::meta::info field_array[] = {Fields...};
        for (std::size_t left = 0; left < sizeof...(Fields); ++left) {
            for (std::size_t right = left + 1; right < sizeof...(Fields); ++right) {
                if (field_array[left] == field_array[right]) {
                    return true;
                }
            }
        }

        return false;
    }
}

template<std::meta::info Model, std::meta::info... Fields>
consteval auto validate_select() -> void {
    if constexpr (sizeof...(Fields) == 0) {
        throw "cpporm select requires at least one field";
    }

    if (!std::meta::is_type(Model) || !std::meta::is_class_type(Model) || !std::meta::is_complete_type(Model)) {
        throw "cpporm select model must be a complete class type";
    }

    if (has_duplicate_fields<Fields...>()) {
        throw "cpporm select fields must not contain duplicates";
    }

    ((is_model_field<Model, Fields>() ? void() : throw "cpporm selected field does not belong to model"), ...);
    ((has_annotation(Fields, ^^ignore) ? throw "cpporm ignored field cannot be selected" : void()), ...);
    ((is_relation_field(Fields) ? throw "cpporm relation field cannot be selected as a scalar field" : void()), ...);
}

template<std::meta::info Model, std::meta::info Field>
consteval auto validate_query_field() -> void {
    if (!is_model_field<Model, Field>()) {
        throw "cpporm query field does not belong to model";
    }

    if (has_annotation(Field, ^^ignore)) {
        throw "cpporm ignored field cannot be used in query clause";
    }
}

consteval auto comparison_operator_sql(comparison_operator op) -> const char* {
    switch (op) {
    case comparison_operator::equal:
        return " = ";
    case comparison_operator::not_equal:
        return " <> ";
    case comparison_operator::less:
        return " < ";
    case comparison_operator::less_equal:
        return " <= ";
    case comparison_operator::greater:
        return " > ";
    case comparison_operator::greater_equal:
        return " >= ";
    case comparison_operator::like:
        return " LIKE ";
    }

    throw "unknown cpporm comparison operator";
}

template<class Dialect>
constexpr auto append_qualified_identifier(sql_string& sql, fixed_string const& table_name, fixed_string const& column_name) -> void {
    Dialect::append_identifier(sql, table_name.view());
    sql.push_back('.');
    Dialect::append_identifier(sql, column_name.view());
}

template<std::meta::info Source, std::meta::info Target>
consteval auto reference_field_to() -> std::meta::info {
    if (!std::meta::is_type(Target) || !std::meta::is_class_type(Target) || !std::meta::is_complete_type(Target)) {
        throw "cpporm join target must be a complete class type";
    }

    std::meta::info local_field{};
    for (auto field : std::meta::nonstatic_data_members_of(
             Source,
             std::meta::access_context::unchecked())) {
        auto target_field = referenced_field_of(field);
        if (target_field == std::meta::info{}) {
            continue;
        }

        if (std::meta::parent_of(target_field) != Target) {
            continue;
        }

        if (local_field != std::meta::info{}) {
            throw "cpporm join target is ambiguous";
        }

        local_field = field;
    }

    if (local_field == std::meta::info{}) {
        throw "cpporm join target has no reference from source model";
    }

    auto target_field = referenced_field_of(local_field);
    if (has_annotation(target_field, ^^ignore)) {
        throw "cpporm join referenced field must not be ignored";
    }

    if (std::meta::type_of(local_field) != std::meta::type_of(target_field)) {
        throw "cpporm join reference field type must match referenced field type";
    }

    return local_field;
}

template<class Dialect, std::meta::info Model, std::meta::info Field, comparison_operator Operator, class Value>
constexpr auto append_predicate_sql(sql_string& sql, comparison_predicate<Field, Operator, Value>) -> void {
    validate_query_field<Model, Field>();
    Dialect::append_identifier(sql, column_name_of(Field).view());
    sql.append(comparison_operator_sql(Operator));
    Dialect::append_parameter(sql);
}

template<class Dialect, std::meta::info Model, class Left, logical_operator Operator, class Right>
constexpr auto append_predicate_sql(sql_string& sql, logical_predicate<Left, Operator, Right> predicate) -> void {
    sql.push_back('(');
    append_predicate_sql<Dialect, Model>(sql, predicate.left);
    if constexpr (Operator == logical_operator::conjunction) {
        sql.append(" AND ");
    } else {
        sql.append(" OR ");
    }
    append_predicate_sql<Dialect, Model>(sql, predicate.right);
    sql.push_back(')');
}

template<class Dialect, std::meta::info Model, class Expression>
constexpr auto append_predicate_sql(sql_string& sql, not_predicate<Expression> predicate) -> void {
    sql.append("NOT (");
    append_predicate_sql<Dialect, Model>(sql, predicate.expression);
    sql.push_back(')');
}

template<std::meta::info Model, std::meta::info... Fields>
struct select_result {
    struct type;

    consteval {
        validate_select<Model, Fields...>();
        std::meta::define_aggregate(^^type, {
            std::meta::data_member_spec(
                std::meta::type_of(Fields),
                {.name = std::meta::identifier_of(Fields)}
            )...
        });
    }
};

template<std::meta::info Model, std::meta::info... Fields>
using select_result_t = typename select_result<Model, Fields...>::type;

template<class Dialect, std::meta::info Model, std::meta::info... Fields>
consteval auto make_select_sql() -> sql_string {
    validate_select<Model, Fields...>();

    sql_string sql{};
    sql.append("SELECT ");

    bool first = true;
    ((first ? void(first = false) : sql.append(", "), Dialect::append_identifier(sql, column_name_of(Fields).view())), ...);

    sql.append(" FROM ");
    Dialect::append_identifier(sql, table_name_of(Model).view());

    return sql;
}

template<class Dialect, std::size_t LimitValue, class Bindings, std::meta::info Model, std::meta::info... Fields>
struct select_query {
    using dialect_type = Dialect;
    using bindings_type = Bindings;
    using row_type = select_result_t<Model, Fields...>;
    using type = row_type;
    static constexpr auto model = Model;
    static constexpr std::size_t field_count = sizeof...(Fields);

    sql_string sql;
    [[no_unique_address]] Bindings bindings;
    bool has_where_clause = false;
    bool has_order_by_clause = false;
    bool has_limit_clause = false;
    bool has_offset_clause = false;

    template<class Target>
    [[nodiscard]] constexpr auto join() const -> select_query {
        if (has_where_clause || has_order_by_clause || has_limit_clause || has_offset_clause) {
            throw "cpporm join must be added before where, order_by, limit, and offset";
        }

        constexpr auto local_field = reference_field_to<Model, ^^Target>();
        constexpr auto target_field = referenced_field_of(local_field);

        auto next = *this;
        next.sql.append(" JOIN ");
        Dialect::append_identifier(next.sql, table_name_of(^^Target).view());
        next.sql.append(" ON ");
        append_qualified_identifier<Dialect>(next.sql, table_name_of(Model), column_name_of(local_field));
        next.sql.append(" = ");
        append_qualified_identifier<Dialect>(next.sql, table_name_of(^^Target), column_name_of(target_field));
        return next;
    }

    template<predicate_expression Expression>
    [[nodiscard]] constexpr auto where(Expression expression) const {
        if (has_order_by_clause || has_limit_clause || has_offset_clause) {
            throw "cpporm where must be added before order_by, limit, and offset";
        }

        using expression_bindings = predicate_bindings_t<Expression>;
        using next_bindings = decltype(std::tuple_cat(
            std::declval<Bindings>(),
            std::declval<expression_bindings>()
        ));

        auto next = select_query<Dialect, LimitValue, next_bindings, Model, Fields...>{
            .sql = sql,
            .bindings = std::tuple_cat(bindings, collect_predicate_bindings(expression)),
            .has_where_clause = has_where_clause,
            .has_order_by_clause = has_order_by_clause,
            .has_limit_clause = has_limit_clause,
            .has_offset_clause = has_offset_clause
        };
        if (next.has_where_clause) {
            next.sql.append(" AND ");
        } else {
            next.sql.append(" WHERE ");
            next.has_where_clause = true;
        }

        append_predicate_sql<Dialect, Model>(next.sql, expression);

        return next;
    }

    template<std::meta::info Field, sort_direction Direction>
    [[nodiscard]] consteval auto order_by(order_expression<Field, Direction>) const -> select_query {
        validate_query_field<Model, Field>();

        if (has_limit_clause || has_offset_clause) {
            throw "cpporm order_by must be added before limit and offset";
        }

        auto next = *this;
        if (next.has_order_by_clause) {
            next.sql.append(", ");
        } else {
            next.sql.append(" ORDER BY ");
            next.has_order_by_clause = true;
        }

        Dialect::append_identifier(next.sql, column_name_of(Field).view());
        if constexpr (Direction == sort_direction::ascending) {
            next.sql.append(" ASC");
        } else {
            next.sql.append(" DESC");
        }

        return next;
    }

    template<std::size_t Value>
    [[nodiscard]] constexpr auto limit() const -> select_query<Dialect, Value, Bindings, Model, Fields...> {
        if (has_limit_clause) {
            throw "cpporm limit was already specified";
        }

        if (has_offset_clause) {
            throw "cpporm limit must be added before offset";
        }

        auto next = select_query<Dialect, Value, Bindings, Model, Fields...>{
            .sql = sql,
            .bindings = bindings,
            .has_where_clause = has_where_clause,
            .has_order_by_clause = has_order_by_clause,
            .has_limit_clause = has_limit_clause,
            .has_offset_clause = has_offset_clause
        };
        next.sql.append(" LIMIT ");
        next.sql.append_unsigned(Value);
        next.has_limit_clause = true;
        return next;
    }

    template<std::size_t Value>
    [[nodiscard]] constexpr auto offset() const -> select_query {
        if (!has_limit_clause) {
            throw "cpporm offset requires limit";
        }

        if (has_offset_clause) {
            throw "cpporm offset was already specified";
        }

        auto next = *this;
        next.sql.append(" OFFSET ");
        next.sql.append_unsigned(Value);
        next.has_offset_clause = true;
        return next;
    }

    template<template<class...> class Container>
    [[nodiscard]] constexpr auto as() const -> sequence_materialization<select_query, Container> {
        return sequence_materialization<select_query, Container>{.query = *this};
    }

    template<template<class, std::size_t> class Container>
    [[nodiscard]] constexpr auto as() const -> fixed_size_materialization<select_query, Container, LimitValue>
        requires (LimitValue != dynamic_extent) {
        return fixed_size_materialization<select_query, Container, LimitValue>{.query = *this};
    }
};

struct nested_relation_plan {
    fixed_string member_name;
    sql_string sql;
};

template<class T>
struct is_field_ref_selection : std::false_type {};

template<std::meta::info Field>
struct is_field_ref_selection<field_ref<Field>> : std::true_type {};

template<class T>
struct is_relation_selection_selection : std::false_type {};

template<std::meta::info RelationField, std::meta::info... Fields>
struct is_relation_selection_selection<relation_selection<RelationField, Fields...>> : std::true_type {};

template<class T>
concept find_many_selection = is_field_ref_selection<std::remove_cvref_t<T>>::value
    || is_relation_selection_selection<std::remove_cvref_t<T>>::value;

template<std::meta::info RelationField, std::meta::info... Fields>
struct relation_selection_result {
    using child_row_type = select_result_t<relation_target_model_of(RelationField), Fields...>;
    using type = std::conditional_t<
        relation_kind_of(RelationField) == relation_kind::has_many,
        std::vector<child_row_type>,
        std::optional<child_row_type>
    >;
};

template<std::meta::info Model, class Selection>
struct find_many_selection_value;

template<std::meta::info Model, std::meta::info Field>
struct find_many_selection_value<Model, field_ref<Field>> {
    using type = typename[:std::meta::type_of(Field):];
};

template<std::meta::info Model, std::meta::info RelationField, std::meta::info... Fields>
struct find_many_selection_value<Model, relation_selection<RelationField, Fields...>> {
    using type = typename relation_selection_result<RelationField, Fields...>::type;
};

template<std::meta::info Model, class Selection>
using find_many_selection_value_t = typename find_many_selection_value<Model, std::remove_cvref_t<Selection>>::type;

template<std::meta::info Model, class... Selections>
struct find_many_result {
    struct type;

    consteval {
        std::meta::define_aggregate(^^type, {
            std::meta::data_member_spec(
                ^^find_many_selection_value_t<Model, Selections>,
                {.name = [] consteval {
                    using selection = std::remove_cvref_t<Selections>;
                    if constexpr (is_field_ref_selection<selection>::value) {
                        return std::meta::identifier_of(selection::field);
                    } else {
                        return std::meta::identifier_of(selection::relation_field);
                    }
                }()}
            )...
        });
    }
};

template<std::meta::info Model, class... Selections>
using find_many_result_t = typename find_many_result<Model, Selections...>::type;

template<class Dialect, std::meta::info Model, find_many_selection... Selections>
struct find_many_query {
    using dialect_type = Dialect;
    using row_type = find_many_result_t<Model, Selections...>;
    using type = row_type;
    static constexpr auto model = Model;

    sql_string root_sql;
    std::size_t relation_count = 0;
    std::array<nested_relation_plan, max_model_relations> relations{};

    template<template<class...> class Container>
    [[nodiscard]] constexpr auto as() const -> sequence_materialization<find_many_query, Container> {
        return sequence_materialization<find_many_query, Container>{.query = *this};
    }
};

template<class Dialect, std::meta::info Model, std::meta::info Field>
consteval auto append_root_select_field(sql_string& sql, bool& first, field_ref<Field>) -> void {
    validate_select<Model, Field>();
    if (first) {
        first = false;
    } else {
        sql.append(", ");
    }

    Dialect::append_identifier(sql, column_name_of(Field).view());
}

template<class Dialect, std::meta::info Model, std::meta::info RelationField, std::meta::info... Fields>
consteval auto append_root_select_field(sql_string&, bool&, relation_selection<RelationField, Fields...>) -> void {}

template<class Dialect, std::meta::info Model, find_many_selection... Selections>
consteval auto make_find_many_root_sql(Selections... selections) -> sql_string {
    sql_string sql{};
    sql.append("SELECT ");

    bool first = true;
    (append_root_select_field<Dialect, Model>(sql, first, selections), ...);
    if (first) {
        throw "cpporm find_many select requires at least one root field";
    }

    sql.append(" FROM ");
    Dialect::append_identifier(sql, table_name_of(Model).view());
    return sql;
}

template<class Dialect, std::meta::info SourceModel, std::meta::info RelationField, std::meta::info... Fields>
consteval auto make_nested_relation_sql() -> sql_string {
    validate_query_field<SourceModel, RelationField>();
    if (!is_relation_field(RelationField)) {
        throw "cpporm nested select target must be a relation field";
    }

    constexpr auto target_model = relation_target_model_of(RelationField);
    validate_select<target_model, Fields...>();

    std::meta::info relation_local_field{};
    std::meta::info relation_target_field{};
    if (relation_kind_of(RelationField) == relation_kind::has_many) {
        relation_local_field = reference_field_to(target_model, SourceModel);
        if (relation_local_field == std::meta::info{}) {
            throw "cpporm has_many nested select target has no reference back to source model";
        }
        relation_target_field = referenced_field_of(relation_local_field);
    } else {
        relation_local_field = reference_field_to(SourceModel, target_model);
        if (relation_local_field == std::meta::info{}) {
            throw "cpporm relation nested select target has no reference from source model";
        }
        relation_target_field = referenced_field_of(relation_local_field);
    }

    sql_string sql{};
    sql.append("SELECT ");
    bool first = true;
    ((first ? void(first = false) : sql.append(", "), Dialect::append_identifier(sql, column_name_of(Fields).view())), ...);
    sql.append(" FROM ");
    Dialect::append_identifier(sql, table_name_of(target_model).view());
    sql.append(" WHERE ");
    if (relation_kind_of(RelationField) == relation_kind::has_many) {
        Dialect::append_identifier(sql, column_name_of(relation_local_field).view());
    } else {
        Dialect::append_identifier(sql, column_name_of(relation_target_field).view());
    }
    sql.append(" IN (");
    Dialect::append_parameter(sql);
    sql.push_back(')');
    return sql;
}

template<class Dialect, std::meta::info Model, std::meta::info Field>
consteval auto append_nested_relation_plan(std::array<nested_relation_plan, max_model_relations>&, std::size_t&, field_ref<Field>) -> void {}

template<class Dialect, std::meta::info Model, std::meta::info RelationField, std::meta::info... Fields>
consteval auto append_nested_relation_plan(
    std::array<nested_relation_plan, max_model_relations>& plans,
    std::size_t& count,
    relation_selection<RelationField, Fields...>
) -> void {
    if (count >= max_model_relations) {
        throw "cpporm find_many select has too many nested relations";
    }

    plans[count] = nested_relation_plan{
        .member_name = fixed_string{std::meta::identifier_of(RelationField)},
        .sql = make_nested_relation_sql<Dialect, Model, RelationField, Fields...>()
    };
    ++count;
}

template<class Dialect, std::meta::info Model>
struct find_many_builder {
    template<find_many_selection... Selections>
    [[nodiscard]] consteval auto select(Selections... selections) const -> find_many_query<Dialect, Model, Selections...> {
        find_many_query<Dialect, Model, Selections...> query{
            .root_sql = make_find_many_root_sql<Dialect, Model>(selections...),
            .relation_count = 0,
            .relations = {}
        };
        (append_nested_relation_plan<Dialect, Model>(query.relations, query.relation_count, selections), ...);
        return query;
    }
};

template<class Dialect, std::meta::info Model, std::meta::info... Fields>
consteval auto select_for() -> select_query<Dialect, dynamic_extent, std::tuple<>, Model, Fields...> {
    return select_query<Dialect, dynamic_extent, std::tuple<>, Model, Fields...>{
        .sql = make_select_sql<Dialect, Model, Fields...>(),
        .bindings = {},
        .has_where_clause = false,
        .has_order_by_clause = false,
        .has_limit_clause = false,
        .has_offset_clause = false
    };
}

template<std::meta::info Model, std::meta::info... Fields>
consteval auto select() -> select_query<sqlite, dynamic_extent, std::tuple<>, Model, Fields...> {
    return sqlite::select<Model, Fields...>();
}

template<class Model, std::meta::info... Fields>
consteval auto select(field_ref<Fields> const&...) -> select_query<sqlite, dynamic_extent, std::tuple<>, ^^Model, Fields...> {
    return select_for<sqlite, ^^Model, Fields...>();
}

template<std::meta::info Model, std::meta::info... Fields>
consteval auto sqlite::select() -> select_query<sqlite, dynamic_extent, std::tuple<>, Model, Fields...> {
    return select_for<sqlite, Model, Fields...>();
}

template<class Model, std::meta::info... Fields>
consteval auto sqlite::select(field_ref<Fields> const&...) -> select_query<sqlite, dynamic_extent, std::tuple<>, ^^Model, Fields...> {
    return select_for<sqlite, ^^Model, Fields...>();
}

template<class Model>
consteval auto sqlite::find_many() -> find_many_builder<sqlite, ^^Model> {
    return {};
}

} // namespace cpporm

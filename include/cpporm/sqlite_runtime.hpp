#pragma once

#include <cpporm/cpporm.hpp>

#include <sqlite3.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cpporm {

class sqlite::database {
public:
    database() = default;

    database(database const&) = delete;
    auto operator=(database const&) -> database& = delete;

    database(database&& other) noexcept
        : connection_(std::exchange(other.connection_, nullptr)) {}

    auto operator=(database&& other) noexcept -> database& {
        if (this != &other) {
            close();
            connection_ = std::exchange(other.connection_, nullptr);
        }
        return *this;
    }

    ~database() {
        close();
    }

    [[nodiscard]] static auto open(std::string_view path) -> std::expected<database, query_error> {
        sqlite3* connection = nullptr;
        auto const result = sqlite3_open_v2(
            std::string(path).c_str(),
            &connection,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            nullptr
        );

        if (result != SQLITE_OK) {
            auto error = make_error(connection, "failed to open sqlite database");
            if (connection != nullptr) {
                sqlite3_close(connection);
            }
            return std::unexpected(error);
        }

        return database(connection);
    }

    [[nodiscard]] static auto open_memory() -> std::expected<database, query_error> {
        return open(":memory:");
    }

    [[nodiscard]] auto execute(std::string_view sql) -> std::expected<void, query_error> {
        char* error_message = nullptr;
        auto const result = sqlite3_exec(connection_, std::string(sql).c_str(), nullptr, nullptr, &error_message);
        if (result != SQLITE_OK) {
            std::string message = error_message == nullptr ? "sqlite execution failed" : error_message;
            sqlite3_free(error_message);
            return std::unexpected(query_error{query_error_kind::execution_failed, store_message(std::move(message))});
        }

        return {};
    }

    [[nodiscard]] auto execute_schema(schema_plan const& schema) -> std::expected<void, query_error> {
        for (std::size_t index = 0; index < schema.statement_count; ++index) {
            auto result = execute(schema.statements[index].view());
            if (!result) {
                return result;
            }
        }

        return {};
    }

    template<class Model>
    [[nodiscard]] auto insert(Model const& value) -> std::expected<void, query_error> {
        return insert<Model, Model>(value);
    }

    template<class Model, class Values>
    [[nodiscard]] auto insert(Values const& value) -> std::expected<void, query_error> {
        return execute_mutation(sqlite::insert_sql<Model, Values>(), [&](sqlite3_stmt* statement) {
            return bind_insert_members<^^Model, ^^Values>(statement, value);
        });
    }

    template<class Model>
    [[nodiscard]] auto update(Model const& value) -> std::expected<void, query_error> {
        return update<Model, Model>(value);
    }

    template<class Model, class Values>
    [[nodiscard]] auto update(Values const& value) -> std::expected<void, query_error> {
        return execute_mutation(sqlite::update_sql<Model, Values>(), [&](sqlite3_stmt* statement) {
            return bind_update_members<^^Model, ^^Values>(statement, value);
        });
    }

    template<class Model>
    [[nodiscard]] auto upsert(Model const& value) -> std::expected<void, query_error> {
        return upsert<Model, Model>(value);
    }

    template<class Model, class Values>
    [[nodiscard]] auto upsert(Values const& value) -> std::expected<void, query_error> {
        return execute_mutation(sqlite::upsert_sql<Model, Values>(), [&](sqlite3_stmt* statement) {
            return bind_insert_members<^^Model, ^^Values>(statement, value);
        });
    }

    template<std::size_t LimitValue, class Bindings, std::meta::info Model, std::meta::info... Fields>
    [[nodiscard]] auto fetch(sequence_materialization<select_query<sqlite, LimitValue, Bindings, Model, Fields...>, std::vector> plan)
        -> typename sequence_materialization<select_query<sqlite, LimitValue, Bindings, Model, Fields...>, std::vector>::result_type {
        using query_type = select_query<sqlite, LimitValue, Bindings, Model, Fields...>;
        using row_type = typename query_type::row_type;

        sqlite3_stmt* raw_statement = nullptr;
        auto const prepare_result = sqlite3_prepare_v2(
            connection_,
            plan.query.sql.view().data(),
            static_cast<int>(plan.query.sql.view().size()),
            &raw_statement,
            nullptr
        );
        statement statement{raw_statement};
        if (prepare_result != SQLITE_OK) {
            return std::unexpected(make_error(connection_, "failed to prepare sqlite statement"));
        }

        if (auto bound = bind_all(raw_statement, plan.query.bindings); !bound) {
            return std::unexpected(bound.error());
        }

        std::vector<row_type> rows{};
        while (true) {
            auto const step_result = sqlite3_step(raw_statement);
            if (step_result == SQLITE_DONE) {
                break;
            }

            if (step_result != SQLITE_ROW) {
                return std::unexpected(make_error(connection_, "failed to step sqlite statement"));
            }

            auto row = read_row<row_type, Fields...>(raw_statement, std::make_index_sequence<sizeof...(Fields)>{});
            if (!row) {
                return std::unexpected(row.error());
            }

            rows.push_back(std::move(*row));
        }

        return rows;
    }

    template<std::meta::info Model, find_many_selection... Selections>
    [[nodiscard]] auto fetch(sequence_materialization<find_many_query<sqlite, Model, Selections...>, std::vector> plan)
        -> typename sequence_materialization<find_many_query<sqlite, Model, Selections...>, std::vector>::result_type {
        using query_type = find_many_query<sqlite, Model, Selections...>;
        using row_type = typename query_type::row_type;

        auto rows = fetch_find_many_root<row_type, Model, Selections...>(plan.query.root_sql);
        if (!rows) {
            return std::unexpected(rows.error());
        }

        auto relation_result = fetch_all_nested_relations<Model, Selections...>(*rows);
        if (!relation_result) {
            return std::unexpected(relation_result.error());
        }

        return *rows;
    }

private:
    explicit database(sqlite3* connection)
        : connection_(connection) {}

    struct statement {
        sqlite3_stmt* value = nullptr;

        ~statement() {
            if (value != nullptr) {
                sqlite3_finalize(value);
            }
        }
    };

    sqlite3* connection_ = nullptr;

    auto close() -> void {
        if (connection_ != nullptr) {
            sqlite3_close(connection_);
            connection_ = nullptr;
        }
    }

    static auto store_message(std::string message) -> std::string_view {
        static std::vector<std::string> messages;
        messages.push_back(std::move(message));
        return messages.back();
    }

    static auto make_error(sqlite3* connection, std::string_view fallback) -> query_error {
        if (connection == nullptr) {
            return query_error{query_error_kind::execution_failed, fallback};
        }

        auto const* message = sqlite3_errmsg(connection);
        if (message == nullptr) {
            return query_error{query_error_kind::execution_failed, fallback};
        }

        return query_error{query_error_kind::execution_failed, store_message(message)};
    }

    static auto append_runtime_identifier(std::string& sql, std::string_view identifier) -> void {
        sql.push_back('"');
        for (char character : identifier) {
            if (character == '"') {
                sql.push_back('"');
            }
            sql.push_back(character);
        }
        sql.push_back('"');
    }

    static auto append_placeholders(std::string& sql, std::size_t count) -> void {
        for (std::size_t index = 0; index < count; ++index) {
            if (index != 0) {
                sql.append(", ");
            }
            sql.push_back('?');
        }
    }

    template<class Row, std::meta::info Field>
    static constexpr bool row_has_member = requires(Row row) {
        row.[:Field:];
    };

    template<class Row, std::size_t Index>
    static consteval auto row_member_at() -> std::meta::info {
        static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(
            ^^Row,
            std::meta::access_context::unchecked()
        ));

        if constexpr (Index >= members.size()) {
            throw "cpporm generated row member index is out of range";
        }

        return members[Index];
    }

    template<std::meta::info Field, class Selection>
    struct selection_matches_field : std::false_type {};

    template<std::meta::info Field, std::meta::info SelectionField>
    struct selection_matches_field<Field, field_ref<SelectionField>> : std::bool_constant<Field == SelectionField> {};

    template<std::meta::info Field, std::size_t Index, class... Selections>
    struct selection_field_index_impl;

    template<std::meta::info Field, std::size_t Index>
    struct selection_field_index_impl<Field, Index> {
        static constexpr std::size_t value = dynamic_extent;
    };

    template<std::meta::info Field, std::size_t Index, class First, class... Rest>
    struct selection_field_index_impl<Field, Index, First, Rest...> {
        static constexpr std::size_t next = selection_field_index_impl<Field, Index + 1, Rest...>::value;
        static constexpr std::size_t value = selection_matches_field<Field, std::remove_cvref_t<First>>::value ? Index : next;
    };

    template<std::meta::info Field, class... Selections>
    static constexpr std::size_t selection_field_index = selection_field_index_impl<Field, 0, Selections...>::value;

    template<std::meta::info RelationField, class Selection>
    struct selection_matches_relation : std::false_type {};

    template<std::meta::info RelationField, std::meta::info SelectionRelationField, std::meta::info... Fields>
    struct selection_matches_relation<RelationField, relation_selection<SelectionRelationField, Fields...>>
        : std::bool_constant<RelationField == SelectionRelationField> {};

    template<std::meta::info RelationField, std::size_t Index, class... Selections>
    struct selection_relation_index_impl;

    template<std::meta::info RelationField, std::size_t Index>
    struct selection_relation_index_impl<RelationField, Index> {
        static constexpr std::size_t value = dynamic_extent;
    };

    template<std::meta::info RelationField, std::size_t Index, class First, class... Rest>
    struct selection_relation_index_impl<RelationField, Index, First, Rest...> {
        static constexpr std::size_t next = selection_relation_index_impl<RelationField, Index + 1, Rest...>::value;
        static constexpr std::size_t value = selection_matches_relation<RelationField, std::remove_cvref_t<First>>::value ? Index : next;
    };

    template<std::meta::info RelationField, class... Selections>
    static constexpr std::size_t selection_relation_index = selection_relation_index_impl<RelationField, 0, Selections...>::value;

    template<std::meta::info ModelInfo, class Selection, std::size_t Index, class Tuple>
    static auto assign_root_selection(sqlite3_stmt* statement, Tuple& values, int& column, query_error& error) -> bool {
        using selection = std::remove_cvref_t<Selection>;
        if constexpr (is_field_ref_selection<selection>::value) {
            constexpr auto field = selection::field;
            using value_type = typename[:std::meta::type_of(field):];
            auto value = read_column<value_type>(statement, column);
            if (!value) {
                error = value.error();
                return false;
            }
            std::get<Index>(values) = std::move(*value);
            ++column;
        }
        return true;
    }

    template<class Row, std::meta::info ModelInfo, class... Selections, std::size_t... Indexes>
    static auto read_find_many_root_row(sqlite3_stmt* statement, std::index_sequence<Indexes...>) -> std::expected<Row, query_error> {
        std::tuple<find_many_selection_value_t<ModelInfo, Selections>...> values{};
        int column = 0;
        query_error error{query_error_kind::conversion_failed, "failed to read sqlite row"};
        bool ok = true;
        ((ok ? ok = assign_root_selection<ModelInfo, Selections, Indexes>(statement, values, column, error) : false), ...);
        if (!ok) {
            return std::unexpected(error);
        }

        return std::apply([](auto&&... items) {
            return Row{std::forward<decltype(items)>(items)...};
        }, std::move(values));
    }

    template<class Row, std::meta::info ModelInfo, class... Selections>
    auto fetch_find_many_root(sql_string const& sql) -> std::expected<std::vector<Row>, query_error> {
        sqlite3_stmt* raw_statement = nullptr;
        auto const prepare_result = sqlite3_prepare_v2(
            connection_,
            sql.view().data(),
            static_cast<int>(sql.view().size()),
            &raw_statement,
            nullptr
        );
        statement statement{raw_statement};
        if (prepare_result != SQLITE_OK) {
            return std::unexpected(make_error(connection_, "failed to prepare sqlite root statement"));
        }

        std::vector<Row> rows{};
        while (true) {
            auto const step_result = sqlite3_step(raw_statement);
            if (step_result == SQLITE_DONE) {
                break;
            }

            if (step_result != SQLITE_ROW) {
                return std::unexpected(make_error(connection_, "failed to step sqlite root statement"));
            }

            auto row = read_find_many_root_row<Row, ModelInfo, Selections...>(raw_statement, std::index_sequence_for<Selections...>{});
            if (!row) {
                return std::unexpected(row.error());
            }

            rows.push_back(std::move(*row));
        }

        return rows;
    }

    template<std::meta::info TargetModel, std::meta::info KeyField, std::meta::info... Fields>
    static auto make_nested_runtime_sql(std::size_t key_count) -> std::string {
        std::string sql;
        sql.append("SELECT ");
        append_runtime_identifier(sql, column_name_of(KeyField).view());
        ((sql.append(", "), append_runtime_identifier(sql, column_name_of(Fields).view())), ...);
        sql.append(" FROM ");
        append_runtime_identifier(sql, table_name_of(TargetModel).view());
        sql.append(" WHERE ");
        append_runtime_identifier(sql, column_name_of(KeyField).view());
        sql.append(" IN (");
        append_placeholders(sql, key_count);
        sql.push_back(')');
        return sql;
    }

    template<class Row, std::meta::info KeyField, class... Selections>
    static auto bind_parent_key(sqlite3_stmt* statement, int& index, Row const& row) -> std::expected<void, query_error> {
        constexpr auto key_index = selection_field_index<KeyField, Selections...>;
        static_assert(key_index != dynamic_extent, "cpporm nested select requires the relation key field in root select");
        constexpr auto row_member = row_member_at<Row, key_index>();
        auto result = bind_one(statement, index, row.[:row_member:]);
        if (result) {
            ++index;
        }
        return result;
    }

    template<class Row, std::meta::info KeyField, class... Selections, class Key>
    static auto parent_key_equals(Row const& row, Key const& key) -> bool {
        constexpr auto key_index = selection_field_index<KeyField, Selections...>;
        static_assert(key_index != dynamic_extent, "cpporm nested select requires the relation key field in root select");
        constexpr auto row_member = row_member_at<Row, key_index>();
        return row.[:row_member:] == key;
    }

    template<class ChildRow, std::meta::info... Fields, std::size_t... Indexes>
    static auto read_nested_child_row(sqlite3_stmt* statement, std::index_sequence<Indexes...>) -> std::expected<ChildRow, query_error> {
        std::tuple<typename[:std::meta::type_of(Fields):]...> values{};
        query_error error{query_error_kind::conversion_failed, "failed to read sqlite nested row"};
        bool ok = true;
        ((ok ? ok = assign_column<Indexes, Fields>(statement, values, error, 1) : false), ...);
        if (!ok) {
            return std::unexpected(error);
        }

        return std::apply([](auto&&... items) {
            return ChildRow{std::forward<decltype(items)>(items)...};
        }, std::move(values));
    }

    template<std::meta::info ModelInfo, class CurrentSelection, class... AllSelections, class Rows>
    auto fetch_nested_relation(Rows& rows) -> std::expected<void, query_error> {
        using selection = std::remove_cvref_t<CurrentSelection>;
        if constexpr (is_relation_selection_selection<selection>::value) {
            return fetch_nested_relation_selection_impl<ModelInfo>(rows, selection{}, std::type_identity<AllSelections>{}...);
        } else {
            return {};
        }
    }

    template<std::meta::info ModelInfo, std::meta::info RelationField, std::meta::info... Fields, class Rows, class... AllSelections>
    auto fetch_nested_relation_selection_impl(
        Rows& rows,
        relation_selection<RelationField, Fields...>,
        std::type_identity<AllSelections>...
    ) -> std::expected<void, query_error> {
        if (rows.empty()) {
            return {};
        }

        constexpr auto target_model = relation_target_model_of(RelationField);
        constexpr bool is_many = relation_kind_of(RelationField) == relation_kind::has_many;
        constexpr auto parent_key_field = [] consteval {
            if constexpr (is_many) {
                return referenced_field_of(reference_field_to(target_model, ModelInfo));
            } else {
                return reference_field_to(ModelInfo, target_model);
            }
        }();
        constexpr auto child_key_field = [] consteval {
            if constexpr (is_many) {
                return reference_field_to(target_model, ModelInfo);
            } else {
                return referenced_field_of(reference_field_to(ModelInfo, target_model));
            }
        }();
        using child_row = select_result_t<target_model, Fields...>;
        using key_type = typename[:std::meta::type_of(child_key_field):];

        auto sql = make_nested_runtime_sql<target_model, child_key_field, Fields...>(rows.size());
        sqlite3_stmt* raw_statement = nullptr;
        auto const prepare_result = sqlite3_prepare_v2(
            connection_,
            sql.data(),
            static_cast<int>(sql.size()),
            &raw_statement,
            nullptr
        );
        statement statement{raw_statement};
        if (prepare_result != SQLITE_OK) {
            return std::unexpected(make_error(connection_, "failed to prepare sqlite nested statement"));
        }

        int bind_index = 1;
        for (auto const& row : rows) {
            auto bound = bind_parent_key<std::remove_cvref_t<decltype(row)>, parent_key_field, AllSelections...>(raw_statement, bind_index, row);
            if (!bound) {
                return std::unexpected(bound.error());
            }
        }

        while (true) {
            auto const step_result = sqlite3_step(raw_statement);
            if (step_result == SQLITE_DONE) {
                break;
            }

            if (step_result != SQLITE_ROW) {
                return std::unexpected(make_error(connection_, "failed to step sqlite nested statement"));
            }

            auto key = read_column<key_type>(raw_statement, 0);
            if (!key) {
                return std::unexpected(key.error());
            }

            auto child = read_nested_child_row<child_row, Fields...>(raw_statement, std::make_index_sequence<sizeof...(Fields)>{});
            if (!child) {
                return std::unexpected(child.error());
            }

            for (auto& row : rows) {
                if (!parent_key_equals<std::remove_cvref_t<decltype(row)>, parent_key_field, AllSelections...>(row, *key)) {
                    continue;
                }

                constexpr auto relation_index = selection_relation_index<RelationField, AllSelections...>;
                static_assert(relation_index != dynamic_extent, "cpporm nested relation selection was not found in generated row");
                constexpr auto row_relation_member = row_member_at<std::remove_cvref_t<decltype(row)>, relation_index>();
                if constexpr (is_many) {
                    row.[:row_relation_member:].push_back(*child);
                } else {
                    row.[:row_relation_member:] = *child;
                }
            }
        }

        return {};
    }

    template<std::meta::info ModelInfo, class... Selections, class Rows>
    auto fetch_all_nested_relations(Rows& rows) -> std::expected<void, query_error> {
        std::expected<void, query_error> result{};
        ((result ? result = fetch_nested_relation<ModelInfo, Selections, Selections...>(rows) : result), ...);
        return result;
    }

    template<class Binder>
    auto execute_mutation(sql_string const& sql, Binder&& binder) -> std::expected<void, query_error> {
        sqlite3_stmt* raw_statement = nullptr;
        auto const prepare_result = sqlite3_prepare_v2(
            connection_,
            sql.view().data(),
            static_cast<int>(sql.view().size()),
            &raw_statement,
            nullptr
        );
        statement statement{raw_statement};
        if (prepare_result != SQLITE_OK) {
            return std::unexpected(make_error(connection_, "failed to prepare sqlite mutation"));
        }

        if (auto bound = binder(raw_statement); !bound) {
            return std::unexpected(bound.error());
        }

        auto const step_result = sqlite3_step(raw_statement);
        if (step_result != SQLITE_DONE) {
            return std::unexpected(make_error(connection_, "failed to execute sqlite mutation"));
        }

        return {};
    }

    template<class T>
    static auto bind_one(sqlite3_stmt* statement, int index, T const& value) -> std::expected<void, query_error> {
        int result = SQLITE_MISUSE;
        if constexpr (std::same_as<std::remove_cvref_t<T>, std::string>) {
            result = sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        } else if constexpr (std::same_as<std::remove_cvref_t<T>, std::string_view>) {
            result = sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        } else if constexpr (std::same_as<std::remove_cvref_t<T>, std::nullptr_t>) {
            result = sqlite3_bind_null(statement, index);
        } else if constexpr (is_optional_v<T>) {
            if (value.has_value()) {
                return bind_one(statement, index, *value);
            }
            result = sqlite3_bind_null(statement, index);
        } else if constexpr (sqlite_blob_value<T>) {
            result = sqlite3_bind_blob(
                statement,
                index,
                value.empty() ? nullptr : value.data(),
                static_cast<int>(value.size() * sizeof(typename std::remove_cvref_t<T>::value_type)),
                SQLITE_TRANSIENT
            );
        } else if constexpr (std::integral<std::remove_cvref_t<T>>) {
            result = sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value));
        } else if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
            result = sqlite3_bind_double(statement, index, static_cast<double>(value));
        } else {
            static_assert(sizeof(T) == 0, "cpporm unsupported sqlite binding type");
        }

        if (result != SQLITE_OK) {
            return std::unexpected(query_error{query_error_kind::execution_failed, "failed to bind sqlite parameter"});
        }

        return {};
    }

    template<class Tuple, std::size_t... Indexes>
    static auto bind_all_impl(sqlite3_stmt* statement, Tuple const& bindings, std::index_sequence<Indexes...>) -> std::expected<void, query_error> {
        std::expected<void, query_error> result{};
        ((result ? result = bind_one(statement, static_cast<int>(Indexes + 1), std::get<Indexes>(bindings)) : result), ...);
        return result;
    }

    template<class... Values>
    static auto bind_all(sqlite3_stmt* statement, std::tuple<Values...> const& bindings) -> std::expected<void, query_error> {
        return bind_all_impl(statement, bindings, std::index_sequence_for<Values...>{});
    }

    template<std::meta::info PayloadField, class Values>
    static auto bind_payload_member(sqlite3_stmt* statement, int& index, Values const& value) -> std::expected<void, query_error> {
        auto result = bind_one(statement, index, value.[:PayloadField:]);
        if (result) {
            ++index;
        }
        return result;
    }

    template<std::meta::info ModelInfo, std::meta::info PayloadInfo, class Values>
    static auto bind_insert_members(sqlite3_stmt* statement, Values const& value) -> std::expected<void, query_error> {
        int index = 1;
        static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
            ModelInfo,
            std::meta::access_context::unchecked()
        ));

        template for (constexpr std::meta::info field : fields) {
            constexpr auto payload_field = payload_field_for<field, PayloadInfo>();
            if constexpr (!is_persisted_field<field>() || payload_field == std::meta::info{}) {
                continue;
            }

            auto result = bind_payload_member<payload_field>(statement, index, value);
            if (!result) {
                return result;
            }
        }

        return {};
    }

    template<std::meta::info ModelInfo, std::meta::info PayloadInfo, class Values>
    static auto bind_update_members(sqlite3_stmt* statement, Values const& value) -> std::expected<void, query_error> {
        constexpr auto primary_key = primary_key_field_of<ModelInfo>();
        constexpr auto primary_key_payload_field = payload_field_for<primary_key, PayloadInfo>();
        int index = 1;
        static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(
            ModelInfo,
            std::meta::access_context::unchecked()
        ));

        template for (constexpr std::meta::info field : fields) {
            constexpr auto payload_field = payload_field_for<field, PayloadInfo>();
            if constexpr (!is_persisted_field<field>() || field == primary_key || payload_field == std::meta::info{}) {
                continue;
            }

            auto result = bind_payload_member<payload_field>(statement, index, value);
            if (!result) {
                return result;
            }
        }

        return bind_payload_member<primary_key_payload_field>(statement, index, value);
    }

    template<class T>
    static auto read_column(sqlite3_stmt* statement, int index) -> std::expected<T, query_error> {
        if constexpr (is_optional_v<T>) {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                return std::optional<optional_value_t<T>>{};
            }

            auto value = read_column<optional_value_t<T>>(statement, index);
            if (!value) {
                return std::unexpected(value.error());
            }

            return std::optional<optional_value_t<T>>{std::move(*value)};
        } else if constexpr (std::same_as<T, std::string>) {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                return std::unexpected(query_error{query_error_kind::conversion_failed, "sqlite column is null"});
            }

            auto const* text = sqlite3_column_text(statement, index);
            auto const size = sqlite3_column_bytes(statement, index);
            return std::string(reinterpret_cast<char const*>(text), static_cast<std::size_t>(size));
        } else if constexpr (sqlite_blob_value<T>) {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                return std::unexpected(query_error{query_error_kind::conversion_failed, "sqlite column is null"});
            }

            auto const* blob = sqlite3_column_blob(statement, index);
            auto const size = sqlite3_column_bytes(statement, index);
            T value(static_cast<std::size_t>(size));
            if (size > 0) {
                std::memcpy(value.data(), blob, static_cast<std::size_t>(size));
            }
            return value;
        } else if constexpr (std::integral<T>) {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                return std::unexpected(query_error{query_error_kind::conversion_failed, "sqlite column is null"});
            }

            return static_cast<T>(sqlite3_column_int64(statement, index));
        } else if constexpr (std::floating_point<T>) {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                return std::unexpected(query_error{query_error_kind::conversion_failed, "sqlite column is null"});
            }

            return static_cast<T>(sqlite3_column_double(statement, index));
        } else {
            static_assert(sizeof(T) == 0, "cpporm unsupported sqlite result column type");
        }
    }

    template<std::size_t Index, std::meta::info Field, class Tuple>
    static auto assign_column(sqlite3_stmt* statement, Tuple& values, query_error& error, int column_offset = 0) -> bool {
        using value_type = typename[:std::meta::type_of(Field):];
        auto value = read_column<value_type>(statement, static_cast<int>(Index) + column_offset);
        if (!value) {
            error = value.error();
            return false;
        }

        std::get<Index>(values) = std::move(*value);
        return true;
    }

    template<class Row, std::meta::info... Fields, std::size_t... Indexes>
    static auto read_row(sqlite3_stmt* statement, std::index_sequence<Indexes...>) -> std::expected<Row, query_error> {
        std::tuple<typename[:std::meta::type_of(Fields):]...> values{};
        query_error error{query_error_kind::conversion_failed, "failed to read sqlite row"};
        bool ok = true;
        ((ok ? ok = assign_column<Indexes, Fields>(statement, values, error) : false), ...);
        if (!ok) {
            return std::unexpected(error);
        }

        return std::apply([](auto&&... items) {
            return Row{std::forward<decltype(items)>(items)...};
        }, std::move(values));
    }
};

} // namespace cpporm

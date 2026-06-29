#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <list>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace models {

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::id{}]]
    std::int64_t id;

    [[=cpporm::column{"display_name"}]]
    std::string name;

    [[=cpporm::unique{}]]
    std::string email;

    [[=cpporm::ignore{}]]
    bool dirty;
};

struct unannotated_user {
    std::int64_t id;
};

enum class role {
    admin
};

inline int not_a_model = 0;

namespace nested {
struct [[=cpporm::table{"projects"}]] project {
    std::int64_t id;
};
} // namespace nested

} // namespace models

using namespace models;

consteval auto basic_registry() {
    return cpporm::register_namespace(^^models);
}

template<class T>
concept has_email = requires(T value) {
    value.email;
};

template<class T>
concept has_dirty = requires(T value) {
    value.dirty;
};

TEST_CASE("register_namespace discovers all direct structs") {
    static_assert(basic_registry().model_count == 2);
    static_assert(basic_registry().models[0].type_name == "user");
    static_assert(basic_registry().models[0].table_name == "users");
    static_assert(basic_registry().models[1].type_name == "unannotated_user");
    static_assert(basic_registry().models[1].table_name == "unannotated_user");
    CHECK(true);
}

TEST_CASE("register_namespace reads field annotations") {
    static_assert(basic_registry().models[0].field_count == 4);

    static_assert(basic_registry().models[0].fields[0].member_name == "id");
    static_assert(basic_registry().models[0].fields[0].column_name == "user_id");
    static_assert(basic_registry().models[0].fields[0].primary_key);
    static_assert(!basic_registry().models[0].fields[0].ignored);

    static_assert(basic_registry().models[0].fields[1].member_name == "name");
    static_assert(basic_registry().models[0].fields[1].column_name == "display_name");
    static_assert(!basic_registry().models[0].fields[1].primary_key);
    static_assert(!basic_registry().models[0].fields[1].ignored);

    static_assert(basic_registry().models[0].fields[2].member_name == "email");
    static_assert(basic_registry().models[0].fields[2].column_name == "email");
    static_assert(!basic_registry().models[0].fields[2].primary_key);
    static_assert(basic_registry().models[0].fields[2].unique);
    static_assert(!basic_registry().models[0].fields[2].ignored);

    static_assert(basic_registry().models[0].fields[3].member_name == "dirty");
    static_assert(basic_registry().models[0].fields[3].column_name == "dirty");
    static_assert(!basic_registry().models[0].fields[3].primary_key);
    static_assert(basic_registry().models[0].fields[3].ignored);
    CHECK(true);
}

TEST_CASE("select builds projection row type") {
    constexpr auto query = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name
    );
    using row = typename decltype(query)::row_type;

    static_assert(requires(row value) { value.id; });
    static_assert(requires(row value) { value.name; });
    static_assert(!has_email<row>);
    static_assert(!has_dirty<row>);
    static_assert(std::meta::nonstatic_data_members_of(^^row, std::meta::access_context::unchecked()).size() == 2);

    CHECK(true);
}

TEST_CASE("select generates sqlite SQL") {
    constexpr auto query = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name,
        cpporm::fields<user>.email
    );
    constexpr std::string_view expected_sql =
        R"(SELECT "user_id", "display_name", "email" FROM "users")";

    CHECK(query.sql == expected_sql);
}

TEST_CASE("sqlite query builder combines clauses") {
    constexpr auto query = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name
    )
        .where(cpporm::fields<user>.email == "tony@example.com")
        .order_by(cpporm::desc(cpporm::fields<user>.name))
        .order_by(cpporm::asc(cpporm::fields<user>.id))
        .limit<10>()
        .offset<20>();

    CHECK(query.sql == R"(SELECT "user_id", "display_name" FROM "users" WHERE "email" = ? ORDER BY "display_name" DESC, "user_id" ASC LIMIT 10 OFFSET 20)");
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view>>);
    CHECK(std::get<0>(query.bindings) == "tony@example.com");
}

TEST_CASE("query exposes projection row type") {
    constexpr auto query = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name
    );

    using row = typename decltype(query)::type;
    static_assert(std::same_as<row, typename decltype(query)::row_type>);
}

TEST_CASE("query materializes as vector") {
    constexpr auto rows = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name
    )
        .as<std::vector>();

    using row = typename decltype(rows.query)::row_type;
    using result = typename decltype(rows)::result_type;

    static_assert(std::same_as<typename decltype(rows)::value_type, std::vector<row>>);
    static_assert(std::same_as<result, std::expected<std::vector<row>, cpporm::query_error>>);
    CHECK(rows.query.sql == R"(SELECT "user_id", "display_name" FROM "users")");
}

TEST_CASE("query materializes as list") {
    constexpr auto rows = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name
    )
        .as<std::list>();

    using row = typename decltype(rows.query)::row_type;
    using result = typename decltype(rows)::result_type;

    static_assert(std::same_as<typename decltype(rows)::value_type, std::list<row>>);
    static_assert(std::same_as<result, std::expected<std::list<row>, cpporm::query_error>>);
    CHECK(rows.query.sql == R"(SELECT "user_id", "display_name" FROM "users")");
}

TEST_CASE("limited query materializes as array") {
    constexpr auto rows = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.name
    )
        .limit<3>()
        .as<std::array>();

    using row = typename decltype(rows.query)::row_type;
    using result = typename decltype(rows)::result_type;

    static_assert(decltype(rows)::expected_size == 3);
    static_assert(std::same_as<typename decltype(rows)::value_type, std::array<row, 3>>);
    static_assert(std::same_as<result, std::expected<std::array<row, 3>, cpporm::query_error>>);
    CHECK(rows.query.sql == R"(SELECT "user_id", "display_name" FROM "users" LIMIT 3)");
}

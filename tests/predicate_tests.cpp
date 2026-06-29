#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace models::predicate {

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}]]
    std::int64_t id;

    [[=cpporm::column{"display_name"}]]
    std::string name;

    std::string email;
};

} // namespace models::predicate

using namespace models::predicate;

TEST_CASE("where supports and predicates") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(cpporm::fields<user>.email == "tony@example.com" && cpporm::fields<user>.id >= 10);

    CHECK(query.sql == R"(SELECT "user_id" FROM "users" WHERE ("email" = ? AND "user_id" >= ?))");
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view, std::int64_t>>);
    CHECK(std::get<0>(query.bindings) == "tony@example.com");
    CHECK(std::get<1>(query.bindings) == 10);
}

TEST_CASE("where supports or predicates") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(cpporm::fields<user>.email == "tony@example.com" || cpporm::fields<user>.name == "Tony");

    CHECK(query.sql == R"(SELECT "user_id" FROM "users" WHERE ("email" = ? OR "display_name" = ?))");
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view, std::string_view>>);
    CHECK(std::get<0>(query.bindings) == "tony@example.com");
    CHECK(std::get<1>(query.bindings) == "Tony");
}

TEST_CASE("where supports not predicates") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(!(cpporm::fields<user>.id < 10));

    CHECK(query.sql == R"(SELECT "user_id" FROM "users" WHERE NOT ("user_id" < ?))");
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::int64_t>>);
    CHECK(std::get<0>(query.bindings) == 10);
}

TEST_CASE("where supports like predicates") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(cpporm::fields<user>.name.like("%Tony%"));

    CHECK(query.sql == R"(SELECT "user_id" FROM "users" WHERE "display_name" LIKE ?)" );
    static_assert(cpporm::predicate_expression<decltype(cpporm::fields<user>.name.like("%Tony%"))>);
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view>>);
    CHECK(std::get<0>(query.bindings) == "%Tony%");
}

TEST_CASE("where supports all and any helpers") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(cpporm::all(
            cpporm::any(
                cpporm::fields<user>.email == "tony@example.com",
                cpporm::fields<user>.name.like("%Tony%")
            ),
            !(cpporm::fields<user>.id < 10)
        ));

    CHECK(query.sql == R"(SELECT "user_id" FROM "users" WHERE (("email" = ? OR "display_name" LIKE ?) AND NOT ("user_id" < ?)))");
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view, std::string_view, std::int64_t>>);
    CHECK(std::get<0>(query.bindings) == "tony@example.com");
    CHECK(std::get<1>(query.bindings) == "%Tony%");
    CHECK(std::get<2>(query.bindings) == 10);
}

TEST_CASE("multiple where clauses are appended with and") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(cpporm::fields<user>.email == "tony@example.com")
        .where(cpporm::fields<user>.id >= 10);

    CHECK(query.sql == R"(SELECT "user_id" FROM "users" WHERE "email" = ? AND "user_id" >= ?)" );
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view, std::int64_t>>);
    CHECK(std::get<0>(query.bindings) == "tony@example.com");
    CHECK(std::get<1>(query.bindings) == 10);
}

TEST_CASE("integer predicate values are stored as field type") {
    constexpr auto query = cpporm::sqlite::select<user>(cpporm::fields<user>.id)
        .where(cpporm::fields<user>.id >= 10u);

    static_assert(cpporm::compatible_field_value<decltype(cpporm::fields<user>.id), int>);
    static_assert(cpporm::compatible_field_value<decltype(cpporm::fields<user>.id), unsigned long long>);
    static_assert(!cpporm::compatible_field_value<decltype(cpporm::fields<user>.id), double>);
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::int64_t>>);
    CHECK(std::get<0>(query.bindings) == 10);
}

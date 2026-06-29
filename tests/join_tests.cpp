#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

namespace models::joins {

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    std::string email;
};

struct [[=cpporm::table{"posts"}]] post {
    [[=cpporm::column{"post_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    [[=cpporm::column{"author_id"}, =cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;

    std::string title;
};

} // namespace models::joins

using namespace models::joins;

TEST_CASE("join uses reference annotation for on clause") {
    constexpr auto query = cpporm::sqlite::select<post>(cpporm::fields<post>.id)
        .join<user>();

    CHECK(query.sql == R"(SELECT "post_id" FROM "posts" JOIN "users" ON "posts"."author_id" = "users"."user_id")");
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<>>);
}

TEST_CASE("join composes before where") {
    constexpr auto query = cpporm::sqlite::select<post>(cpporm::fields<post>.id)
        .join<user>()
        .where(cpporm::fields<post>.title == "Hello");

    CHECK(query.sql == R"(SELECT "post_id" FROM "posts" JOIN "users" ON "posts"."author_id" = "users"."user_id" WHERE "title" = ?)" );
    static_assert(std::same_as<decltype(query)::bindings_type, std::tuple<std::string_view>>);
    CHECK(std::get<0>(query.bindings) == "Hello");
}

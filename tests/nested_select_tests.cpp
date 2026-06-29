#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>

namespace models::nested_select {

struct post;

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    std::string email;

    cpporm::has_many<post> posts;
};

struct [[=cpporm::table{"posts"}]] post {
    [[=cpporm::column{"post_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    [[=cpporm::column{"author_id"}, =cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;

    std::string title;

    cpporm::relation<user> author;
};

} // namespace models::nested_select

using namespace models::nested_select;

TEST_CASE("find_many supports has_many nested select") {
    constexpr auto query = cpporm::sqlite::find_many<user>()
        .select(
            cpporm::fields<user>.id,
            cpporm::fields<user>.email,
            cpporm::relations<user>.posts.select(
                cpporm::fields<post>.id,
                cpporm::fields<post>.title
            )
        );

    CHECK(query.root_sql == R"(SELECT "user_id", "email" FROM "users")");
    REQUIRE(query.relation_count == 1);
    CHECK(query.relations[0].member_name == "posts");
    CHECK(query.relations[0].sql == R"(SELECT "post_id", "title" FROM "posts" WHERE "author_id" IN (?))");
}

TEST_CASE("find_many supports relation nested select") {
    constexpr auto query = cpporm::sqlite::find_many<post>()
        .select(
            cpporm::fields<post>.id,
            cpporm::fields<post>.title,
            cpporm::relations<post>.author.select(
                cpporm::fields<user>.id,
                cpporm::fields<user>.email
            )
        );

    CHECK(query.root_sql == R"(SELECT "post_id", "title" FROM "posts")");
    REQUIRE(query.relation_count == 1);
    CHECK(query.relations[0].member_name == "author");
    CHECK(query.relations[0].sql == R"(SELECT "user_id", "email" FROM "users" WHERE "user_id" IN (?))");
}

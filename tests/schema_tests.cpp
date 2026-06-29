#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace models::schema {

struct post;

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    std::string email;

    [[=cpporm::ignore{}]]
    bool dirty;

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

struct [[=cpporm::table{"samples"}]] sample {
    [[=cpporm::column{"sample_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    int flag;
    double score;
    std::optional<std::string> nickname;
    std::vector<std::byte> payload;
};

} // namespace models::schema

using namespace models::schema;

constexpr auto schema_plan = cpporm::sqlite::create_schema<^^models::schema>();

constexpr auto schema_contains(std::string_view sql) -> bool {
    for (std::size_t index = 0; index < schema_plan.statement_count; ++index) {
        if (schema_plan.statements[index] == sql) {
            return true;
        }
    }

    return false;
}

TEST_CASE("sqlite create_table generates scalar columns") {
    constexpr auto sql = cpporm::sqlite::create_table<user>();

    CHECK(sql == R"(CREATE TABLE "users" ("user_id" INTEGER PRIMARY KEY, "email" TEXT NOT NULL))");
}

TEST_CASE("sqlite create_table generates foreign keys") {
    constexpr auto sql = cpporm::sqlite::create_table<post>();

    CHECK(sql == R"(CREATE TABLE "posts" ("post_id" INTEGER PRIMARY KEY, "author_id" INTEGER NOT NULL, "title" TEXT NOT NULL, FOREIGN KEY ("author_id") REFERENCES "users" ("user_id")))");
}

TEST_CASE("sqlite create_table_if_not_exists is safe for existing tables") {
    constexpr auto sql = cpporm::sqlite::create_table_if_not_exists<user>();

    CHECK(sql == R"(CREATE TABLE IF NOT EXISTS "users" ("user_id" INTEGER PRIMARY KEY, "email" TEXT NOT NULL))");
}

TEST_CASE("sqlite create_table supports all storage classes") {
    constexpr auto sql = cpporm::sqlite::create_table<sample>();

    CHECK(sql == R"(CREATE TABLE "samples" ("sample_id" INTEGER PRIMARY KEY, "flag" INTEGER NOT NULL, "score" REAL NOT NULL, "nickname" TEXT, "payload" BLOB NOT NULL))");
}

TEST_CASE("sqlite create_schema generates all direct model tables") {
    static_assert(schema_plan.statement_count == 3);
    static_assert(schema_contains(R"(CREATE TABLE IF NOT EXISTS "users" ("user_id" INTEGER PRIMARY KEY, "email" TEXT NOT NULL))"));
    static_assert(schema_contains(R"(CREATE TABLE IF NOT EXISTS "posts" ("post_id" INTEGER PRIMARY KEY, "author_id" INTEGER NOT NULL, "title" TEXT NOT NULL, FOREIGN KEY ("author_id") REFERENCES "users" ("user_id")))"));
    static_assert(schema_contains(R"(CREATE TABLE IF NOT EXISTS "samples" ("sample_id" INTEGER PRIMARY KEY, "flag" INTEGER NOT NULL, "score" REAL NOT NULL, "nickname" TEXT, "payload" BLOB NOT NULL))"));

    CHECK(true);
}

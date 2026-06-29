#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>

namespace models::mutations {

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    std::string email;

    [[=cpporm::ignore{}]]
    bool dirty;
};

struct user_patch {
    std::int64_t id;
    std::string email;
    int extra;
};

struct email_patch {
    std::string email;
};

} // namespace models::mutations

using namespace models::mutations;

TEST_CASE("sqlite insert_sql uses payload fields") {
    constexpr auto sql = cpporm::sqlite::insert_sql<user, email_patch>();

    CHECK(sql == R"(INSERT INTO "users" ("email") VALUES (?))");
}

TEST_CASE("sqlite update_sql ignores extra payload fields") {
    constexpr auto sql = cpporm::sqlite::update_sql<user, user_patch>();

    CHECK(sql == R"(UPDATE "users" SET "email" = ? WHERE "user_id" = ?)" );
}

TEST_CASE("sqlite upsert_sql uses payload fields") {
    constexpr auto sql = cpporm::sqlite::upsert_sql<user, user_patch>();

    CHECK(sql == R"(INSERT INTO "users" ("user_id", "email") VALUES (?, ?) ON CONFLICT ("user_id") DO UPDATE SET "email" = excluded."email")");
}

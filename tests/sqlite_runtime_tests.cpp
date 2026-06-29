#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cpporm/sqlite_runtime.hpp>

#include <cstdint>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace runtime_models {

struct [[=cpporm::table{"users"}]] user {
    [[=cpporm::column{"user_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    std::string email;
};

struct [[=cpporm::table{"samples"}]] sample {
    [[=cpporm::column{"sample_id"}, =cpporm::primary_key{}]]
    std::int64_t id;

    int flag;
    double score;
    std::optional<std::string> nickname;
    std::vector<std::byte> payload;
};

} // namespace runtime_models

namespace runtime_nested_models {

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

} // namespace runtime_nested_models

using namespace runtime_models;

TEST_CASE("sqlite runtime fetches flat vector query") {
    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());

    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_models>()).has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_models>()).has_value());
    REQUIRE(database->execute(R"(
        INSERT INTO users (user_id, email) VALUES
            (1, 'tony@example.com'),
            (2, 'alice@example.com');
    )").has_value());

    constexpr auto query = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.email
    )
        .where(cpporm::fields<user>.id >= 2)
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 1);
    CHECK((*rows)[0].id == 2);
    CHECK((*rows)[0].email == "alice@example.com");
}

TEST_CASE("sqlite runtime reports execution errors") {
    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());

    auto result = database->execute("SELECT * FROM missing_table");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == cpporm::query_error_kind::execution_failed);
}

TEST_CASE("sqlite runtime reads nullable values and blobs") {
    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_models>()).has_value());
    REQUIRE(database->execute(R"(
        INSERT INTO samples (sample_id, flag, score, nickname, payload) VALUES
            (1, 7, 3.5, 'Ada', X'0102'),
            (2, 8, 4.5, NULL, X'FF');
    )").has_value());

    auto query = cpporm::sqlite::select<sample>(
        cpporm::fields<sample>.id,
        cpporm::fields<sample>.flag,
        cpporm::fields<sample>.score,
        cpporm::fields<sample>.nickname,
        cpporm::fields<sample>.payload
    )
        .where(cpporm::fields<sample>.payload == std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}})
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 1);
    CHECK((*rows)[0].id == 1);
    CHECK((*rows)[0].flag == 7);
    CHECK((*rows)[0].score == doctest::Approx(3.5));
    REQUIRE((*rows)[0].nickname.has_value());
    CHECK(*(*rows)[0].nickname == "Ada");
    CHECK((*rows)[0].payload == std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}});
}

TEST_CASE("sqlite runtime reads null optional columns") {
    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_models>()).has_value());
    REQUIRE(database->execute(R"(
        INSERT INTO samples (sample_id, flag, score, nickname, payload) VALUES
            (1, 8, 4.5, NULL, X'FF');
    )").has_value());

    constexpr auto query = cpporm::sqlite::select<sample>(
        cpporm::fields<sample>.id,
        cpporm::fields<sample>.nickname,
        cpporm::fields<sample>.payload
    )
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 1);
    CHECK((*rows)[0].id == 1);
    CHECK_FALSE((*rows)[0].nickname.has_value());
    CHECK((*rows)[0].payload == std::vector<std::byte>{std::byte{0xff}});
}

TEST_CASE("sqlite runtime binds optional values") {
    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_models>()).has_value());
    REQUIRE(database->execute(R"(
        INSERT INTO samples (sample_id, flag, score, nickname, payload) VALUES
            (1, 7, 3.5, 'Ada', X'0102'),
            (2, 8, 4.5, NULL, X'FF');
    )").has_value());

    auto query = cpporm::sqlite::select<sample>(cpporm::fields<sample>.id)
        .where(cpporm::fields<sample>.nickname == std::optional<std::string>{"Ada"})
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 1);
    CHECK((*rows)[0].id == 1);

    auto null_query = cpporm::sqlite::select<sample>(cpporm::fields<sample>.id)
        .where(cpporm::fields<sample>.nickname == std::optional<std::string>{})
        .as<std::vector>();

    auto null_rows = database->fetch(null_query);
    REQUIRE(null_rows.has_value());
    CHECK(null_rows->empty());
}

TEST_CASE("sqlite runtime inserts updates and upserts arbitrary payload structs") {
    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_models>()).has_value());

    struct {
        std::int64_t id;
        std::string email;
        int ignored_extra;
    } insert_payload{1, "tony@example.com", 123};

    REQUIRE(database->insert<user>(insert_payload).has_value());

    struct {
        std::int64_t id;
        std::string email;
    } update_payload{1, "updated@example.com"};

    REQUIRE(database->update<user>(update_payload).has_value());

    struct {
        std::int64_t id;
        std::string email;
    } upsert_existing{1, "upserted@example.com"};

    REQUIRE(database->upsert<user>(upsert_existing).has_value());

    struct {
        std::int64_t id;
        std::string email;
    } upsert_new{2, "new@example.com"};

    REQUIRE(database->upsert<user>(upsert_new).has_value());

    constexpr auto query = cpporm::sqlite::select<user>(
        cpporm::fields<user>.id,
        cpporm::fields<user>.email
    )
        .order_by(cpporm::asc(cpporm::fields<user>.id))
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 2);
    CHECK((*rows)[0].id == 1);
    CHECK((*rows)[0].email == "upserted@example.com");
    CHECK((*rows)[1].id == 2);
    CHECK((*rows)[1].email == "new@example.com");
}

TEST_CASE("sqlite runtime fetches has_many nested select") {
    using nested_user = runtime_nested_models::user;
    using nested_post = runtime_nested_models::post;

    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_nested_models>()).has_value());
    REQUIRE(database->execute(R"(
        INSERT INTO users (user_id, email) VALUES
            (1, 'tony@example.com'),
            (2, 'alice@example.com'),
            (3, 'empty@example.com');
        INSERT INTO posts (post_id, author_id, title) VALUES
            (10, 1, 'first'),
            (11, 1, 'second'),
            (12, 2, 'third');
    )").has_value());

    constexpr auto query = cpporm::sqlite::find_many<nested_user>()
        .select(
            cpporm::fields<nested_user>.id,
            cpporm::fields<nested_user>.email,
            cpporm::relations<nested_user>.posts.select(
                cpporm::fields<nested_post>.id,
                cpporm::fields<nested_post>.title
            )
        )
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 3);
    CHECK((*rows)[0].id == 1);
    CHECK((*rows)[0].email == "tony@example.com");
    REQUIRE((*rows)[0].posts.size() == 2);
    CHECK((*rows)[0].posts[0].id == 10);
    CHECK((*rows)[0].posts[0].title == "first");
    CHECK((*rows)[0].posts[1].id == 11);
    CHECK((*rows)[0].posts[1].title == "second");
    REQUIRE((*rows)[1].posts.size() == 1);
    CHECK((*rows)[1].posts[0].id == 12);
    CHECK((*rows)[1].posts[0].title == "third");
    CHECK((*rows)[2].posts.empty());
}

TEST_CASE("sqlite runtime fetches relation nested select") {
    using nested_user = runtime_nested_models::user;
    using nested_post = runtime_nested_models::post;

    auto database = cpporm::sqlite::database::open_memory();
    REQUIRE(database.has_value());
    REQUIRE(database->execute_schema(cpporm::sqlite::create_schema<^^runtime_nested_models>()).has_value());
    REQUIRE(database->execute(R"(
        INSERT INTO users (user_id, email) VALUES
            (1, 'tony@example.com'),
            (2, 'alice@example.com');
        INSERT INTO posts (post_id, author_id, title) VALUES
            (10, 1, 'first'),
            (11, 2, 'second'),
            (12, 99, 'orphan');
    )").has_value());

    constexpr auto query = cpporm::sqlite::find_many<nested_post>()
        .select(
            cpporm::fields<nested_post>.id,
            cpporm::fields<nested_post>.user_id,
            cpporm::fields<nested_post>.title,
            cpporm::relations<nested_post>.author.select(
                cpporm::fields<nested_user>.id,
                cpporm::fields<nested_user>.email
            )
        )
        .as<std::vector>();

    auto rows = database->fetch(query);
    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 3);
    REQUIRE((*rows)[0].author.has_value());
    CHECK((*rows)[0].author->id == 1);
    CHECK((*rows)[0].author->email == "tony@example.com");
    REQUIRE((*rows)[1].author.has_value());
    CHECK((*rows)[1].author->id == 2);
    CHECK((*rows)[1].author->email == "alice@example.com");
    CHECK_FALSE((*rows)[2].author.has_value());
}

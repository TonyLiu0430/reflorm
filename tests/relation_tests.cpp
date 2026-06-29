#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>

namespace models::relations {

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

} // namespace models::relations

using namespace models::relations;

template<class T>
concept has_posts_field = requires(T value) {
    value.posts;
};

template<class T>
concept has_author_relation = requires(T value) {
    value.author;
};

consteval auto relation_registry() {
    return cpporm::register_namespace(^^models::relations);
}

consteval auto model_named(std::string_view name) {
    auto registry = relation_registry();
    for (std::size_t index = 0; index < registry.model_count; ++index) {
        if (registry.models[index].type_name == name) {
            return registry.models[index];
        }
    }

    throw "missing test model";
}

static_assert(relation_registry().model_count == 2);

static_assert(model_named("user").field_count == 2);
static_assert(model_named("user").relation_count == 1);
static_assert(model_named("user").relations[0].member_name == "posts");
static_assert(model_named("user").relations[0].kind == cpporm::relation_kind::has_many);
static_assert(model_named("user").relations[0].target_model_name == "post");
static_assert(model_named("user").relations[0].target_table_name == "posts");
static_assert(model_named("user").relations[0].local_member_name == "user_id");
static_assert(model_named("user").relations[0].local_column_name == "author_id");
static_assert(model_named("user").relations[0].target_member_name == "id");
static_assert(model_named("user").relations[0].target_column_name == "user_id");

static_assert(model_named("post").field_count == 3);
static_assert(model_named("post").relation_count == 1);
static_assert(model_named("post").relations[0].member_name == "author");
static_assert(model_named("post").relations[0].kind == cpporm::relation_kind::relation);
static_assert(model_named("post").relations[0].target_model_name == "user");
static_assert(model_named("post").relations[0].target_table_name == "users");
static_assert(model_named("post").relations[0].local_member_name == "user_id");
static_assert(model_named("post").relations[0].local_column_name == "author_id");
static_assert(model_named("post").relations[0].target_member_name == "id");
static_assert(model_named("post").relations[0].target_column_name == "user_id");

TEST_CASE("fields exclude relation marker fields") {
    static_assert(!has_posts_field<decltype(cpporm::fields<user>)>);
    CHECK(true);
}

TEST_CASE("relations expose relation marker fields") {
    static_assert(has_posts_field<decltype(cpporm::relations<user>)>);
    static_assert(has_author_relation<decltype(cpporm::relations<post>)>);
    CHECK(true);
}

TEST_CASE("register_namespace reads relation metadata") {
    CHECK(true);
}

#include <doctest/doctest.h>

#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>

namespace models::relationships {

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

} // namespace models::relationships

using namespace models::relationships;

consteval auto relationship_registry() {
    return cpporm::register_namespace(^^models::relationships);
}

static_assert(relationship_registry().model_count == 2);
static_assert(relationship_registry().models[1].type_name == "post");
static_assert(relationship_registry().models[1].table_name == "posts");
static_assert(relationship_registry().models[1].field_count == 3);
static_assert(relationship_registry().models[1].fields[1].member_name == "user_id");
static_assert(relationship_registry().models[1].fields[1].column_name == "author_id");
static_assert(relationship_registry().models[1].fields[1].has_reference);
static_assert(relationship_registry().models[1].fields[1].referenced_model_name == "user");
static_assert(relationship_registry().models[1].fields[1].referenced_table_name == "users");
static_assert(relationship_registry().models[1].fields[1].referenced_member_name == "id");
static_assert(relationship_registry().models[1].fields[1].referenced_column_name == "user_id");
static_assert(relationship_registry().models[0].field_count == 2);
static_assert(!relationship_registry().models[0].fields[0].has_reference);
static_assert(!relationship_registry().models[0].fields[1].has_reference);

TEST_CASE("register_namespace reads reference annotations") {
    CHECK(true);
}

TEST_CASE("fields without reference annotations are not relationships") {
    CHECK(true);
}

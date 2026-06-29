#include <cpporm/cpporm.hpp>

#include <cstdint>

namespace invalid_models {

struct post;

struct user {
    std::int64_t id;
    cpporm::has_many<post> posts;
};

struct post {
    std::int64_t id;

    [[=cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;
};

} // namespace invalid_models

using namespace invalid_models;

constexpr auto query = cpporm::sqlite::find_many<user>()
    .select(cpporm::relations<user>.posts.select(cpporm::fields<post>.id));

#include <cpporm/cpporm.hpp>

#include <cstdint>

namespace invalid_models {

struct user {
    std::int64_t id;
};

struct post {
    std::int64_t id;

    [[=cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t author_id;

    [[=cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t editor_id;
};

} // namespace invalid_models

using namespace invalid_models;

constexpr auto query = cpporm::sqlite::select<post>(cpporm::fields<post>.id)
    .join<user>();

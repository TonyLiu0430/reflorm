#include <cpporm/cpporm.hpp>

#include <cstdint>

namespace invalid_models {

struct user {
    std::int64_t id;
};

struct post {
    std::int64_t id;
};

} // namespace invalid_models

using namespace invalid_models;

constexpr auto query = cpporm::sqlite::select<post>(cpporm::fields<post>.id)
    .join<user>();

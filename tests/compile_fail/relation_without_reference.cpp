#include <cpporm/cpporm.hpp>

#include <cstdint>

namespace invalid_models {

struct user {
    std::int64_t id;
};

struct post {
    std::int64_t id;
    cpporm::relation<user> author;
};

} // namespace invalid_models

static_assert(cpporm::register_namespace(^^invalid_models).model_count == 2);

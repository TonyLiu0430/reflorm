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
};

} // namespace invalid_models

static_assert(cpporm::register_namespace(^^invalid_models).model_count == 2);

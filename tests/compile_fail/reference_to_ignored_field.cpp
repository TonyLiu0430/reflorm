#include <cpporm/cpporm.hpp>

#include <cstdint>

namespace invalid_models {

struct user {
    [[=cpporm::ignore{}]]
    std::int64_t id;
};

struct post {
    [[=cpporm::references<cpporm::fields<user>.id>{}]]
    std::int64_t user_id;
};

} // namespace invalid_models

static_assert(cpporm::register_namespace(^^invalid_models).model_count == 2);

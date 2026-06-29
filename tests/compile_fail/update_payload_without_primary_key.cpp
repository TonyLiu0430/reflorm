#include <cpporm/cpporm.hpp>

#include <cstdint>
#include <string>

namespace invalid_models {

struct user {
    [[=cpporm::primary_key{}]]
    std::int64_t id;

    std::string email;
};

struct user_patch {
    std::string email;
};

} // namespace invalid_models

using namespace invalid_models;

constexpr auto sql = cpporm::sqlite::update_sql<user, user_patch>();

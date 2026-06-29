#include <cpporm/cpporm.hpp>

#include <cstdint>

namespace bad_models {

struct project {
    std::int64_t tenant_id;
    std::int64_t slug;

    [[=cpporm::id{"tenant_id", "slug"}]]
    cpporm::model_constraint _id;
};

} // namespace bad_models

constexpr auto sql = cpporm::sqlite::create_table<bad_models::project>();

int main() {}

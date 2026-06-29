#pragma once

#include <cstddef>

namespace cpporm {

inline constexpr std::size_t max_name_size = 128;
inline constexpr std::size_t max_sql_size = 1024;
inline constexpr std::size_t max_registered_models = 64;
inline constexpr std::size_t max_model_fields = 64;
inline constexpr std::size_t max_model_relations = 64;
inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

} // namespace cpporm

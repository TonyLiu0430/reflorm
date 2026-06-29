#pragma once

#include <meta>

namespace cpporm {

template<std::meta::info Field>
struct field_ref;

enum class sort_direction {
    ascending,
    descending
};

template<std::meta::info Field, sort_direction Direction>
struct order_expression {
    static constexpr auto field = Field;
    static constexpr auto direction = Direction;
};

template<std::meta::info Field>
consteval auto asc() -> order_expression<Field, sort_direction::ascending> {
    return {};
}

template<std::meta::info Field>
consteval auto asc(field_ref<Field> const&) -> order_expression<Field, sort_direction::ascending> {
    return {};
}

template<std::meta::info Field>
consteval auto desc() -> order_expression<Field, sort_direction::descending> {
    return {};
}

template<std::meta::info Field>
consteval auto desc(field_ref<Field> const&) -> order_expression<Field, sort_direction::descending> {
    return {};
}

} // namespace cpporm

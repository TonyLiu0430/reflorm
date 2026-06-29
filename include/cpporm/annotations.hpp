#pragma once

#include <cpporm/config.hpp>
#include <cpporm/fixed_string.hpp>

#include <array>
#include <cstddef>
#include <meta>

namespace cpporm {

template<std::meta::info Field>
struct field_ref;

struct table {
    fixed_string name;

    template<std::size_t N>
    consteval table(char const (&table_name)[N])
        : name(table_name) {}
};

struct column {
    fixed_string name;

    template<std::size_t N>
    consteval column(char const (&column_name)[N])
        : name(column_name) {}
};

struct constraint_fields {
    std::size_t field_count = 0;
    std::array<fixed_string, max_constraint_fields> fields{};

    consteval constraint_fields() = default;

    template<std::size_t... Sizes>
    consteval constraint_fields(char const (&...field_names)[Sizes]) {
        static_assert(sizeof...(Sizes) <= max_constraint_fields, "cpporm constraint has too many fields");

        field_count = sizeof...(Sizes);
        std::size_t index = 0;
        ((fields[index++] = fixed_string{field_names}), ...);
    }
};

struct primary_key {};
struct id : constraint_fields {
    using constraint_fields::constraint_fields;
};

struct unique : constraint_fields {
    using constraint_fields::constraint_fields;
};

struct ignore {};

struct model_constraint {};

template<auto TargetField>
struct references {
    static constexpr auto field = TargetField.field;
};

} // namespace cpporm

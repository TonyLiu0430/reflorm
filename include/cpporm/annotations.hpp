#pragma once

#include <cpporm/fixed_string.hpp>

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

struct primary_key {};
struct ignore {};

template<auto TargetField>
struct references {
    static constexpr auto field = TargetField.field;
};

} // namespace cpporm

#pragma once

#include <cpporm/config.hpp>
#include <cpporm/fixed_string.hpp>

#include <array>
#include <cstddef>

namespace cpporm {

enum class relation_kind;

struct field_descriptor {
    fixed_string member_name;
    fixed_string column_name;
    bool primary_key = false;
    bool ignored = false;
    bool has_reference = false;
    fixed_string referenced_model_name;
    fixed_string referenced_table_name;
    fixed_string referenced_member_name;
    fixed_string referenced_column_name;
};

struct relation_descriptor {
    fixed_string member_name;
    relation_kind kind{};
    fixed_string target_model_name;
    fixed_string target_table_name;
    fixed_string local_member_name;
    fixed_string local_column_name;
    fixed_string target_member_name;
    fixed_string target_column_name;
};

struct model_descriptor {
    fixed_string type_name;
    fixed_string table_name;
    std::size_t field_count = 0;
    std::array<field_descriptor, max_model_fields> fields{};
    std::size_t relation_count = 0;
    std::array<relation_descriptor, max_model_relations> relations{};
};

struct registry_descriptor {
    std::size_t model_count = 0;
    std::array<model_descriptor, max_registered_models> models{};
};

} // namespace cpporm

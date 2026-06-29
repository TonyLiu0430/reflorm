#pragma once

#include <meta>
#include <type_traits>
#include <vector>

namespace cpporm {

template<std::meta::info Field>
struct field_ref;

template<std::meta::info RelationField, std::meta::info... Fields>
struct relation_selection {
    static constexpr auto relation_field = RelationField;
};

template<class Target>
struct relation {};

template<class Target>
struct has_many {};

enum class relation_kind {
    relation,
    has_many
};

template<std::meta::info Field>
struct relation_ref {
    static constexpr auto field = Field;

    template<std::meta::info... Fields>
    consteval auto select(field_ref<Fields> const&...) const -> relation_selection<Field, Fields...> {
        return {};
    }
};

template<class T>
struct is_relation_field_type : std::false_type {};

template<class Target>
struct is_relation_field_type<relation<Target>> : std::true_type {};

template<class Target>
struct is_relation_field_type<has_many<Target>> : std::true_type {};

template<class T>
concept relation_field_type = is_relation_field_type<std::remove_cvref_t<T>>::value;

template<std::meta::info Model>
struct model_relations {
    struct type;

    consteval {
        if (!std::meta::is_type(Model) || !std::meta::is_class_type(Model) || !std::meta::is_complete_type(Model)) {
            throw "cpporm relations model must be a complete class type";
        }

        std::vector<std::meta::info> specs{};
        static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(
            Model,
            std::meta::access_context::unchecked()
        ));
        template for (constexpr std::meta::info member : members) {
            if (!std::meta::has_identifier(member)) {
                continue;
            }

            using member_type = typename[:std::meta::type_of(member):];
            if constexpr (!relation_field_type<member_type>) {
                continue;
            }

            specs.push_back(std::meta::data_member_spec(
                ^^relation_ref<member>,
                {.name = std::meta::identifier_of(member)}
            ));
        }

        std::meta::define_aggregate(^^type, specs);
    }
};

template<std::meta::info Model>
using model_relations_t = typename model_relations<Model>::type;

template<class Model>
inline constexpr model_relations_t<^^Model> relations{};

} // namespace cpporm

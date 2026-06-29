#pragma once

#include <meta>
#include <concepts>
#include <cpporm/relations.hpp>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cpporm {

enum class comparison_operator {
    equal,
    not_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    like
};

enum class logical_operator {
    conjunction,
    disjunction
};

template<std::meta::info Field, comparison_operator Operator, class Value = void>
struct comparison_predicate {
    static constexpr auto field = Field;
    static constexpr auto op = Operator;
    using value_type = Value;

    [[no_unique_address]] Value value;
};

template<std::meta::info Field, comparison_operator Operator>
struct comparison_predicate<Field, Operator, void> {
    static constexpr auto field = Field;
    static constexpr auto op = Operator;
    using value_type = void;
};

struct parameter_placeholder {};

inline constexpr parameter_placeholder param{};

template<std::meta::info Field, class T>
struct predicate_value_storage_for;

template<std::meta::info Field, class T>
using predicate_value_storage_for_t = typename predicate_value_storage_for<Field, T>::type;

template<std::meta::info Field>
struct field_ref {
    static constexpr auto field = Field;

    template<class T>
    constexpr auto like(T&& value) const
        -> comparison_predicate<Field, comparison_operator::like, predicate_value_storage_for_t<Field, T&&>>;
};

template<std::meta::info Field>
inline constexpr field_ref<Field> field{};

template<std::meta::info Model>
struct model_fields {
    struct type;

    consteval {
        if (!std::meta::is_type(Model) || !std::meta::is_class_type(Model) || !std::meta::is_complete_type(Model)) {
            throw "cpporm fields model must be a complete class type";
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
            if constexpr (relation_field_type<member_type>) {
                continue;
            }

            specs.push_back(std::meta::data_member_spec(
                ^^field_ref<member>,
                {.name = std::meta::identifier_of(member)}
            ));
        }

        std::meta::define_aggregate(^^type, specs);
    }
};

template<std::meta::info Model>
using model_fields_t = typename model_fields<Model>::type;

template<class Model>
inline constexpr model_fields_t<^^Model> fields{};

template<class T>
struct is_predicate_expression : std::false_type {};

template<std::meta::info Field, comparison_operator Operator, class Value>
struct is_predicate_expression<comparison_predicate<Field, Operator, Value>> : std::true_type {};

template<class T>
concept predicate_expression = is_predicate_expression<std::remove_cvref_t<T>>::value;

template<std::meta::info Field>
consteval auto operator==(field_ref<Field>, parameter_placeholder) -> comparison_predicate<Field, comparison_operator::equal> {
    return {};
}

template<std::meta::info Field>
consteval auto operator!=(field_ref<Field>, parameter_placeholder) -> comparison_predicate<Field, comparison_operator::not_equal> {
    return {};
}

template<std::meta::info Field>
consteval auto operator<(field_ref<Field>, parameter_placeholder) -> comparison_predicate<Field, comparison_operator::less> {
    return {};
}

template<std::meta::info Field>
consteval auto operator<=(field_ref<Field>, parameter_placeholder) -> comparison_predicate<Field, comparison_operator::less_equal> {
    return {};
}

template<std::meta::info Field>
consteval auto operator>(field_ref<Field>, parameter_placeholder) -> comparison_predicate<Field, comparison_operator::greater> {
    return {};
}

template<std::meta::info Field>
consteval auto operator>=(field_ref<Field>, parameter_placeholder) -> comparison_predicate<Field, comparison_operator::greater_equal> {
    return {};
}

template<class T>
struct predicate_value_storage {
    using type = std::decay_t<T>;
};

template<std::size_t N>
struct predicate_value_storage<char const (&)[N]> {
    using type = std::string_view;
};

template<class T>
using predicate_value_storage_t = typename predicate_value_storage<T>::type;

template<std::meta::info Field>
using field_value_type_t = typename[:std::meta::type_of(Field):];

template<std::meta::info Field, class T>
consteval auto stores_as_field_integral() -> bool {
    return std::meta::is_integral_type(std::meta::dealias(std::meta::type_of(Field)))
        && std::integral<std::remove_cvref_t<T>>;
}

template<std::meta::info Field, class T>
consteval auto rejects_integral_field_value() -> bool {
    return std::meta::is_integral_type(std::meta::dealias(std::meta::type_of(Field)))
        && !std::integral<std::remove_cvref_t<T>>;
}

template<std::meta::info Field, class T>
struct predicate_value_storage_for {
    using type = std::conditional_t<
        stores_as_field_integral<Field, T>(),
        field_value_type_t<Field>,
        predicate_value_storage_t<T>
    >;
};

template<std::meta::info Field, class T>
concept compatible_predicate_value = !rejects_integral_field_value<Field, T>();

template<class Field, class T>
struct is_compatible_field_value : std::false_type {};

template<std::meta::info Field, class T>
struct is_compatible_field_value<field_ref<Field>, T> : std::bool_constant<compatible_predicate_value<Field, T>> {};

template<class Field, class T>
concept compatible_field_value = is_compatible_field_value<std::remove_cvref_t<Field>, T>::value;

template<class T>
constexpr auto store_predicate_value(T&& value) -> predicate_value_storage_t<T&&> {
    if constexpr (std::is_array_v<std::remove_reference_t<T>>) {
        return std::string_view{value, sizeof(value) - 1};
    } else {
        return std::forward<T>(value);
    }
}

template<std::meta::info Field, class T>
constexpr auto store_predicate_value_for_field(T&& value) -> predicate_value_storage_for_t<Field, T&&> {
    static_assert(!rejects_integral_field_value<Field, T&&>(),
        "cpporm integer fields can only be compared with integral values");

    if constexpr (stores_as_field_integral<Field, T&&>()) {
        return static_cast<field_value_type_t<Field>>(value);
    } else {
        return store_predicate_value(std::forward<T>(value));
    }
}

template<class Left, logical_operator Operator, class Right>
struct logical_predicate {
    [[no_unique_address]] Left left;
    [[no_unique_address]] Right right;
};

template<class Expression>
struct not_predicate {
    [[no_unique_address]] Expression expression;
};

template<class Left, logical_operator Operator, class Right>
struct is_predicate_expression<logical_predicate<Left, Operator, Right>> : std::true_type {};

template<class Expression>
struct is_predicate_expression<not_predicate<Expression>> : std::true_type {};

template<predicate_expression Left, predicate_expression Right>
constexpr auto operator&&(Left left, Right right)
    -> logical_predicate<std::remove_cvref_t<Left>, logical_operator::conjunction, std::remove_cvref_t<Right>> {
    return {.left = left, .right = right};
}

template<predicate_expression Left, predicate_expression Right>
constexpr auto operator||(Left left, Right right)
    -> logical_predicate<std::remove_cvref_t<Left>, logical_operator::disjunction, std::remove_cvref_t<Right>> {
    return {.left = left, .right = right};
}

template<predicate_expression Expression>
constexpr auto operator!(Expression expression) -> not_predicate<std::remove_cvref_t<Expression>> {
    return {.expression = expression};
}

template<predicate_expression... Expressions>
    requires (sizeof...(Expressions) > 0)
constexpr auto all(Expressions... expressions) {
    return (expressions && ...);
}

template<predicate_expression... Expressions>
    requires (sizeof...(Expressions) > 0)
constexpr auto any(Expressions... expressions) {
    return (expressions || ...);
}

template<class Expression>
struct predicate_bindings;

template<std::meta::info Field, comparison_operator Operator>
struct predicate_bindings<comparison_predicate<Field, Operator, void>> {
    using type = std::tuple<>;
};

template<std::meta::info Field, comparison_operator Operator, class Value>
struct predicate_bindings<comparison_predicate<Field, Operator, Value>> {
    using type = std::tuple<Value>;
};

template<class Left, logical_operator Operator, class Right>
struct predicate_bindings<logical_predicate<Left, Operator, Right>> {
    using type = decltype(std::tuple_cat(
        std::declval<typename predicate_bindings<Left>::type>(),
        std::declval<typename predicate_bindings<Right>::type>()
    ));
};

template<class Expression>
struct predicate_bindings<not_predicate<Expression>> {
    using type = typename predicate_bindings<Expression>::type;
};

template<class Expression>
using predicate_bindings_t = typename predicate_bindings<std::remove_cvref_t<Expression>>::type;

template<std::meta::info Field, comparison_operator Operator>
constexpr auto collect_predicate_bindings(comparison_predicate<Field, Operator, void>) {
    return std::tuple{};
}

template<std::meta::info Field, comparison_operator Operator, class Value>
constexpr auto collect_predicate_bindings(comparison_predicate<Field, Operator, Value> predicate) {
    return std::tuple<Value>{predicate.value};
}

template<class Left, logical_operator Operator, class Right>
constexpr auto collect_predicate_bindings(logical_predicate<Left, Operator, Right> predicate) {
    return std::tuple_cat(
        collect_predicate_bindings(predicate.left),
        collect_predicate_bindings(predicate.right)
    );
}

template<class Expression>
constexpr auto collect_predicate_bindings(not_predicate<Expression> predicate) {
    return collect_predicate_bindings(predicate.expression);
}

template<std::meta::info Field, class T>
    requires (!std::same_as<std::remove_cvref_t<T>, parameter_placeholder>)
        && compatible_predicate_value<Field, T&&>
constexpr auto operator==(field_ref<Field>, T&& value)
    -> comparison_predicate<Field, comparison_operator::equal, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field, class T>
    requires (!std::same_as<std::remove_cvref_t<T>, parameter_placeholder>)
        && compatible_predicate_value<Field, T&&>
constexpr auto operator!=(field_ref<Field>, T&& value)
    -> comparison_predicate<Field, comparison_operator::not_equal, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field, class T>
    requires (!std::same_as<std::remove_cvref_t<T>, parameter_placeholder>)
        && compatible_predicate_value<Field, T&&>
constexpr auto operator<(field_ref<Field>, T&& value)
    -> comparison_predicate<Field, comparison_operator::less, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field, class T>
    requires (!std::same_as<std::remove_cvref_t<T>, parameter_placeholder>)
        && compatible_predicate_value<Field, T&&>
constexpr auto operator<=(field_ref<Field>, T&& value)
    -> comparison_predicate<Field, comparison_operator::less_equal, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field, class T>
    requires (!std::same_as<std::remove_cvref_t<T>, parameter_placeholder>)
        && compatible_predicate_value<Field, T&&>
constexpr auto operator>(field_ref<Field>, T&& value)
    -> comparison_predicate<Field, comparison_operator::greater, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field, class T>
    requires (!std::same_as<std::remove_cvref_t<T>, parameter_placeholder>)
        && compatible_predicate_value<Field, T&&>
constexpr auto operator>=(field_ref<Field>, T&& value)
    -> comparison_predicate<Field, comparison_operator::greater_equal, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field>
template<class T>
constexpr auto field_ref<Field>::like(T&& value) const
    -> comparison_predicate<Field, comparison_operator::like, predicate_value_storage_for_t<Field, T&&>> {
    return {.value = store_predicate_value_for_field<Field>(std::forward<T>(value))};
}

template<std::meta::info Field>
consteval auto eq() -> comparison_predicate<Field, comparison_operator::equal> {
    return {};
}

template<std::meta::info Field>
consteval auto ne() -> comparison_predicate<Field, comparison_operator::not_equal> {
    return {};
}

template<std::meta::info Field>
consteval auto lt() -> comparison_predicate<Field, comparison_operator::less> {
    return {};
}

template<std::meta::info Field>
consteval auto le() -> comparison_predicate<Field, comparison_operator::less_equal> {
    return {};
}

template<std::meta::info Field>
consteval auto gt() -> comparison_predicate<Field, comparison_operator::greater> {
    return {};
}

template<std::meta::info Field>
consteval auto ge() -> comparison_predicate<Field, comparison_operator::greater_equal> {
    return {};
}

} // namespace cpporm

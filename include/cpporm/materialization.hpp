#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <iterator>
#include <string_view>
#include <vector>

namespace cpporm {

enum class query_error_kind {
    row_count_mismatch,
    execution_failed,
    conversion_failed
};

struct query_error {
    query_error_kind kind;
    std::string_view message;
};

template<class T>
concept begin_end_range = requires(T value) {
    std::begin(value);
    std::end(value);
};

template<class Query, template<class...> class Container>
    requires begin_end_range<Container<typename Query::type>>
struct sequence_materialization {
    using query_type = Query;
    using row_type = typename Query::type;
    using value_type = Container<row_type>;
    using result_type = std::expected<value_type, query_error>;

    Query query;
};

template<class Query, template<class, std::size_t> class Container, std::size_t Size>
    requires begin_end_range<Container<typename Query::type, Size>>
struct fixed_size_materialization {
    using query_type = Query;
    using row_type = typename Query::type;
    using value_type = Container<row_type, Size>;
    using result_type = std::expected<value_type, query_error>;

    static constexpr auto expected_size = Size;

    Query query;
};

} // namespace cpporm

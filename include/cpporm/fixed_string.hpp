#pragma once

#include <cpporm/config.hpp>

#include <cstddef>
#include <string_view>

namespace cpporm {

struct fixed_string {
    char value[max_name_size]{};
    std::size_t size = 0;

    constexpr fixed_string() = default;

    template<std::size_t N>
    consteval fixed_string(char const (&text)[N]) {
        static_assert(N <= max_name_size, "cpporm metadata string is too long");

        size = N - 1;
        for (std::size_t index = 0; index < N; ++index) {
            value[index] = text[index];
        }
    }

    consteval fixed_string(std::string_view text) {
        if (text.size() >= max_name_size) {
            throw "cpporm metadata string is too long";
        }

        size = text.size();
        for (std::size_t index = 0; index < text.size(); ++index) {
            value[index] = text[index];
        }
    }

    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return {value, size};
    }
};

[[nodiscard]] constexpr auto operator==(fixed_string const& lhs, std::string_view rhs) -> bool {
    return lhs.view() == rhs;
}

[[nodiscard]] constexpr auto operator==(std::string_view lhs, fixed_string const& rhs) -> bool {
    return lhs == rhs.view();
}

} // namespace cpporm

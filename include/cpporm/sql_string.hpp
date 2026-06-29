#pragma once

#include <cpporm/config.hpp>
#include <cpporm/fixed_string.hpp>

#include <cstddef>
#include <string_view>

namespace cpporm {

struct sql_string {
    char value[max_sql_size]{};
    std::size_t size = 0;

    constexpr auto push_back(char character) -> void {
        if (size >= max_sql_size) {
            throw "cpporm SQL string is too long";
        }

        value[size] = character;
        ++size;
    }

    constexpr auto append(std::string_view text) -> void {
        for (char character : text) {
            push_back(character);
        }
    }

    template<std::size_t N>
    constexpr auto append(char const (&text)[N]) -> void {
        append(std::string_view{text, N - 1});
    }

    constexpr auto append(fixed_string const& text) -> void {
        append(text.view());
    }

    constexpr auto append_unsigned(std::size_t value) -> void {
        char digits[20]{};
        std::size_t count = 0;

        do {
            digits[count] = static_cast<char>('0' + (value % 10));
            value /= 10;
            ++count;
        } while (value != 0);

        while (count > 0) {
            --count;
            push_back(digits[count]);
        }
    }

    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return {value, size};
    }
};

[[nodiscard]] constexpr auto operator==(sql_string const& lhs, std::string_view rhs) -> bool {
    return lhs.view() == rhs;
}

[[nodiscard]] constexpr auto operator==(sql_string const& lhs, sql_string const& rhs) -> bool {
    return lhs.view() == rhs.view();
}

[[nodiscard]] constexpr auto operator==(std::string_view lhs, sql_string const& rhs) -> bool {
    return lhs == rhs.view();
}

} // namespace cpporm

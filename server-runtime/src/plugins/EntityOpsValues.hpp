#pragma once

#include <basalt/server/plugins/PluginEntityOps.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace basalt::server::runtime::plugins::entityOps::values {

using Value = server::plugins::PluginEntityValue;
using Arguments = std::vector<Value>;

[[nodiscard]] inline bool asBool(const Value& value, bool fallback = false) {
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole != 0;
    }
    return fallback;
}

[[nodiscard]] inline std::int64_t asInt(const Value& value, std::int64_t fallback = 0) {
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole;
    }
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return static_cast<std::int64_t>(*real);
    }
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag ? 1 : 0;
    }
    return fallback;
}

[[nodiscard]] inline double asReal(const Value& value, double fallback = 0.0) {
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return *real;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    return fallback;
}

[[nodiscard]] inline std::optional<std::string> asText(const Value& value) {
    const auto* text = std::get_if<std::string>(&value);
    return text == nullptr ? std::nullopt : std::optional{*text};
}

[[nodiscard]] inline const Value& at(const Arguments& arguments, std::size_t index) {
    static const Value kNone;
    return index < arguments.size() ? arguments[index] : kNone;
}

} // namespace basalt::server::runtime::plugins::entityOps::values

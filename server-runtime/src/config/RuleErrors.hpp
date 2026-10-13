#pragma once

#include <basalt/wire/common/toml/Toml.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config::ruleErrors {

inline std::optional<std::string>*& sink() {
    static thread_local std::optional<std::string>* current = nullptr;
    return current;
}

class Scope {
public:
    Scope() : previous_(sink()) { sink() = &error_; }

    ~Scope() { sink() = previous_; }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    [[nodiscard]] const std::optional<std::string>& error() const noexcept { return error_; }

private:
    std::optional<std::string>* previous_;
    std::optional<std::string> error_;
};

inline void report(std::string_view key, const std::string& why) {
    if (sink() != nullptr && !sink()->has_value()) {
        *sink() = std::string{key} + " " + why;
    }
}

[[nodiscard]] inline std::int32_t whole(const wire::common::toml::TomlTable& table, std::string_view key,
                                        std::int32_t fallback, std::int64_t least, std::int64_t most) {
    if (table.find(key) == nullptr) {
        return fallback;
    }
    const std::optional<std::int64_t> value = table.integer(key);
    if (!value.has_value() || *value < least || *value > most) {
        report(key, "must be a whole number from " + std::to_string(least) + " to " + std::to_string(most));
        return fallback;
    }
    return static_cast<std::int32_t>(*value);
}

[[nodiscard]] inline double real(const wire::common::toml::TomlTable& table, std::string_view key, double fallback,
                                 double least, double most) {
    if (table.find(key) == nullptr) {
        return fallback;
    }
    const std::optional<double> value = table.real(key);
    if (!value.has_value() || !std::isfinite(*value) || *value < least || *value > most) {
        report(key, "must be a finite number from " + std::to_string(least) + " to " + std::to_string(most));
        return fallback;
    }
    return *value;
}

[[nodiscard]] inline bool flag(const wire::common::toml::TomlTable& table, std::string_view key, bool fallback) {
    if (table.find(key) == nullptr) {
        return fallback;
    }
    const std::optional<bool> value = table.boolean(key);
    if (!value.has_value()) {
        report(key, "must be true or false");
        return fallback;
    }
    return *value;
}

[[nodiscard]] inline bool flag(const wire::common::toml::TomlTable* table, std::string_view key, bool fallback) {
    return flag(*table, key, fallback);
}

} // namespace basalt::server::runtime::config::ruleErrors

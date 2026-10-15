#pragma once

#include <basalt/server/plugins/PluginManifest.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
enum class PluginArgumentType : std::uint8_t { Word, Text, Int, Float, Bool };

[[nodiscard]] std::string_view spellingOf(PluginArgumentType type);

[[nodiscard]] std::optional<PluginArgumentType> argumentTypeOf(std::string_view spelling);

struct PluginCommandArgument {
    std::string name;
    PluginArgumentType type = PluginArgumentType::Word;
    bool optional = false;
    std::optional<std::int64_t> min;
    std::optional<std::int64_t> max;
};

struct PluginCommandSpec {
    std::vector<std::string> literals;
    std::vector<PluginCommandArgument> arguments;
    std::optional<std::string> permission;

    [[nodiscard]] const std::string& name() const { return literals.front(); }

    [[nodiscard]] std::string usage() const;

    [[nodiscard]] static std::expected<PluginCommandSpec, ManifestError>
    parse(std::string_view spec, std::optional<std::string> permission = std::nullopt);
};
} // namespace basalt::server::plugins

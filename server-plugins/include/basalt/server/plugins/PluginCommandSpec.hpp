#pragma once

#include <basalt/server/plugins/PluginManifest.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

/// The argument types a plugin can ask for today.
///
/// The list is short on purpose: these are the ones that need nothing from the world. The rest of
/// vanilla's types (players, positions, items, blocks) arrive with the object model, and asking for
/// one before then fails at load with a message that says so rather than at the first use.
enum class PluginArgumentType : std::uint8_t { Word, Text, Int, Float, Bool };

[[nodiscard]] std::string_view spellingOf(PluginArgumentType type);

[[nodiscard]] std::optional<PluginArgumentType> argumentTypeOf(std::string_view spelling);

/// What a plugin asked for in one argument of a command.
struct PluginCommandArgument {
    std::string name;
    PluginArgumentType type = PluginArgumentType::Word;
    bool optional = false;
    std::optional<std::int64_t> min;
    std::optional<std::int64_t> max;
};

/// One command of a plugin, as the loader understood it.
struct PluginCommandSpec {
    /// `arena create` is two: the command and one literal below it.
    std::vector<std::string> literals;
    std::vector<PluginCommandArgument> arguments;
    std::optional<std::string> permission;

    [[nodiscard]] const std::string& name() const { return literals.front(); }

    /// What the plugin wrote, put back together for logs and `/plugins`.
    [[nodiscard]] std::string usage() const;

    /// Reads `arena create <name: word> [size: int 8..64]`.
    ///
    /// One string rather than a builder because the shape of a command is the one thing every plugin
    /// gets right by copying, and a tree built call by call is the part of Bukkit nobody enjoys.
    [[nodiscard]] static std::expected<PluginCommandSpec, ManifestError>
    parse(std::string_view spec, std::optional<std::string> permission = std::nullopt);
};

} // namespace basalt::server::plugins

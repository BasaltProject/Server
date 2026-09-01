#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::config {

/// One command's overrides. Everything is absent when the file left it alone.
struct CommandOverride {
    std::optional<std::string> permission;
    bool enabled = true;
    std::vector<std::string> aliases;

    friend bool operator==(const CommandOverride&, const CommandOverride&) = default;
};

/// A command the operator invented: a name, a permission, and lines to answer with.
struct CustomCommand {
    std::string name;
    std::string permission;
    std::vector<std::string> message;
    std::vector<std::string> aliases;

    friend bool operator==(const CustomCommand&, const CustomCommand&) = default;
};

/// `config/commands.toml`.
///
/// Deliberately three small things rather than one general mechanism: rename a permission, switch a
/// command off, add a name for it; invent a command that answers with text; replace what a command
/// says. Anything past that is a plugin, and pretending otherwise turns a configuration file into a
/// bad programming language.
class CommandsConfig {
public:
    static constexpr std::string_view kFileName = "commands.toml";

    CommandsConfig(std::vector<std::pair<std::string, CommandOverride>> overrides, std::vector<CustomCommand> custom,
                   std::vector<std::pair<std::string, std::string>> messages)
        : overrides_(std::move(overrides)), custom_(std::move(custom)), messages_(std::move(messages)) {}

    [[nodiscard]] static std::expected<CommandsConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    /// What the file says about one command, or the untouched defaults when it says nothing.
    [[nodiscard]] const CommandOverride& get(std::string_view command) const;

    [[nodiscard]] const std::vector<CustomCommand>& custom() const noexcept { return custom_; }

    /// What a command should say instead of what it says by default, by message key.
    [[nodiscard]] const std::string* message(std::string_view key) const;

    [[nodiscard]] const std::vector<std::pair<std::string, std::string>>& messages() const noexcept {
        return messages_;
    }

private:
    std::vector<std::pair<std::string, CommandOverride>> overrides_;
    std::vector<CustomCommand> custom_;
    std::vector<std::pair<std::string, std::string>> messages_;
};

} // namespace basalt::server::runtime::config

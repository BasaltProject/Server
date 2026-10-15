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

struct CommandOverride {
    std::optional<std::string> permission;
    bool enabled = true;
    std::vector<std::string> aliases;

    friend bool operator==(const CommandOverride&, const CommandOverride&) = default;
};

struct CustomCommand {
    std::string name;
    std::string permission;
    std::vector<std::string> message;
    std::vector<std::string> aliases;

    friend bool operator==(const CustomCommand&, const CustomCommand&) = default;
};

class CommandsConfig {
public:
    static constexpr std::string_view kFileName = "commands.toml";

    CommandsConfig(std::vector<std::pair<std::string, CommandOverride>> overrides, std::vector<CustomCommand> custom,
                   std::vector<std::pair<std::string, std::string>> messages)
        : overrides_(std::move(overrides)), custom_(std::move(custom)), messages_(std::move(messages)) {}

    [[nodiscard]] static std::expected<CommandsConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const CommandOverride& get(std::string_view command) const;

    [[nodiscard]] const std::vector<CustomCommand>& custom() const noexcept { return custom_; }

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

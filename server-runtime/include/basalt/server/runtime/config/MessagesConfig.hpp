#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>

#include <expected>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

struct MessagesConfig {
    static constexpr std::string_view kFileName = "messages.toml";

    /// MiniMessage by key; a key that is absent or empty keeps the vanilla translatable text.
    std::map<std::string, std::string, std::less<>> kick;

    [[nodiscard]] const std::string* find(std::string_view key) const;

    [[nodiscard]] static std::expected<MessagesConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{});
};

} // namespace basalt::server::runtime::config

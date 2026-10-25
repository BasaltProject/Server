#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/MessagesConfig.hpp>

#include <array>

namespace basalt::server::runtime::config {
namespace {

constexpr std::array<std::string_view, 12> kKnownKeys{"server-full",
                                                      "not-whitelisted",
                                                      "banned",
                                                      "banned-ip",
                                                      "idle",
                                                      "flying",
                                                      "spam",
                                                      "packet-rate",
                                                      "transfers-disabled",
                                                      "connection-throttled",
                                                      "server-closed",
                                                      "outdated-client"};

} // namespace

const std::string* MessagesConfig::find(std::string_view key) const {
    const auto found = kick.find(key);
    return found == kick.end() || found->second.empty() ? nullptr : &found->second;
}

std::expected<MessagesConfig, std::string> MessagesConfig::load(const ConfigDirectory& directory) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<wire::common::toml::TomlTable, std::string> root =
        directory.load(kFileName, kMessagesDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    MessagesConfig config;
    const wire::common::toml::TomlTable* kicks = root->table("kick");
    if (kicks == nullptr) {
        return config;
    }
    for (const std::string& key : kicks->keys()) {
        bool known = false;
        for (const std::string_view candidate : kKnownKeys) {
            known = known || candidate == key;
        }
        if (!known) {
            return std::unexpected(path + ": kick." + key + " is not a message this server sends");
        }
        const std::optional<std::string_view> text = kicks->string(key);
        if (!text.has_value()) {
            return std::unexpected(path + ": kick." + key + " must be a string");
        }
        config.kick.emplace(key, std::string{*text});
    }
    return config;
}

} // namespace basalt::server::runtime::config

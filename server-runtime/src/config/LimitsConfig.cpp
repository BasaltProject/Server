#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/LimitsConfig.hpp>
#include <basalt/server/runtime/plugins/PacketNames.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;

[[nodiscard]] std::expected<std::int32_t, std::string>
ranged(const TomlTable& table, std::string_view key, std::int32_t fallback, std::int64_t high, std::string_view path) {
    if (table.find(key) != nullptr && !table.integer(key).has_value()) {
        return std::unexpected(std::string{path} + ": " + std::string{key} + " must be a whole number");
    }
    const std::int64_t value = table.integer(key).value_or(fallback);
    if (value < 0 || value > high) {
        return std::unexpected(std::string{path} + ": " + std::string{key} + " must be between 0 and " +
                               std::to_string(high));
    }
    return static_cast<std::int32_t>(value);
}

} // namespace

std::expected<LimitsConfig, std::string> LimitsConfig::load(const ConfigDirectory& directory,
                                                            const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, kLimitsDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    LimitsConfig config;
    if (const TomlTable* packets = root->table("packets"); packets != nullptr) {
        const auto rate = ranged(*packets, "rate-limit", config.packetRateLimit, 1 << 24, path);
        if (!rate.has_value()) {
            return std::unexpected(rate.error());
        }
        config.packetRateLimit = *rate;
        for (const PacketRule::Action action : {PacketRule::Action::Drop, PacketRule::Action::Kick}) {
            const std::string section = action == PacketRule::Action::Drop ? "drop" : "kick";
            const TomlTable* rules = packets->table(section);
            if (rules == nullptr) {
                continue;
            }
            for (const std::string& name : rules->keys()) {
                const std::optional<server::plugins::PacketAddress> address = plugins::packetNames::resolve(name);
                if (!address.has_value() || !address->serverbound) {
                    return std::unexpected(path + ": packets." + section + "." + name +
                                           " is not a packet a client sends");
                }
                const std::optional<std::int64_t> limit = rules->integer(name);
                if (!limit.has_value() || *limit < 1 || *limit > (1 << 24)) {
                    return std::unexpected(path + ": packets." + section + "." + name +
                                           " must be a number of packets per second above 0");
                }
                config.packetRules.push_back(PacketRule{.name = name,
                                                        .address = *address,
                                                        .maxPerSecond = static_cast<std::int32_t>(*limit),
                                                        .action = action});
            }
        }
    }
    if (const TomlTable* spam = root->table("spam"); spam != nullptr) {
        const auto chat = ranged(*spam, "chat-threshold-seconds", config.chatSpamThresholdSeconds, 3600, path);
        const auto command = ranged(*spam, "command-threshold-seconds", config.commandSpamThresholdSeconds, 3600, path);
        if (!chat.has_value() || !command.has_value()) {
            return std::unexpected(!chat.has_value() ? chat.error() : command.error());
        }
        for (std::string name : spam->stringList("excluded-commands")) {
            std::erase(name, '/');
            config.spamExcludedCommands.push_back(wire::common::placeholder::lowercaseAscii(name));
        }
        config.chatSpamThresholdSeconds = *chat;
        config.commandSpamThresholdSeconds = *command;
    }
    if (const TomlTable* items = root->table("items"); items != nullptr) {
        const auto name = ranged(*items, "custom-name-max-length", 0, 1 << 20, path);
        const auto lore = ranged(*items, "lore-line-max-length", 0, 1 << 20, path);
        const auto page = ranged(*items, "book-page-max-length", 0, 1 << 20, path);
        const auto title = ranged(*items, "book-title-max-length", 0, 1 << 20, path);
        for (const auto* checked : {&name, &lore, &page, &title}) {
            if (!checked->has_value()) {
                return std::unexpected(checked->error());
            }
        }
        config.items = ItemCaps{
            .customNameLength = *name, .loreLineLength = *lore, .bookPageLength = *page, .bookTitleLength = *title};
    }
    if (!config.packetRules.empty()) {
        logger.info([&config, &path] {
            return "Loaded " + std::to_string(config.packetRules.size()) + " packet rule(s) from " + path + ".";
        });
    }
    return config;
}

} // namespace basalt::server::runtime::config

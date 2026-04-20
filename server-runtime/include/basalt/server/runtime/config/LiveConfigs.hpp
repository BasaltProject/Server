#pragma once

#include <basalt/server/runtime/config/AnimationsConfig.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>
#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <array>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

struct ConfigSnapshot {
    GroupsConfig groups;
    ServerListConfig serverList;
    CommandsConfig commands;
    AnimationsConfig animations;
    UiConfigs ui;
};

class LiveConfigs {
public:
    static constexpr std::array<std::string_view, 7> kReloadableFiles{
        GroupsConfig::kFileName,  ServerListConfig::kFileName, CommandsConfig::kFileName, AnimationsConfig::kFileName,
        TabListConfig::kFileName, ScoreboardConfig::kFileName, BossBarConfig::kFileName};

    explicit LiveConfigs(ConfigSnapshot initial)
        : snapshot_(std::make_shared<const ConfigSnapshot>(std::move(initial))) {}

    [[nodiscard]] std::shared_ptr<const ConfigSnapshot> current() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return snapshot_;
    }

    void replace(ConfigSnapshot next) {
        // Built before taking the lock so readers never wait on the copy.
        std::shared_ptr<const ConfigSnapshot> replacement = std::make_shared<const ConfigSnapshot>(std::move(next));
        const std::lock_guard<std::mutex> lock{mutex_};
        snapshot_ = std::move(replacement);
    }

    [[nodiscard]] static std::expected<ConfigSnapshot, std::string>
    read(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const ConfigSnapshot> snapshot_;
};

} // namespace basalt::server::runtime::config

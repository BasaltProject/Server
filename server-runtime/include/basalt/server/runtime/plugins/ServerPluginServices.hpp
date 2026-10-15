#pragma once

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {
class CommandDispatcher;
class CommandNode;
class CommandSource;
} // namespace basalt::server::runtime::command

namespace basalt::server::runtime::player {
class PlayerRegistry;
}

namespace basalt::server::runtime::plugins {

class OwnerTasks final : public server::plugins::PluginTasks {
public:
    OwnerTasks(sched::Owner& owner, std::function<std::int64_t()> currentTick)
        : owner_(&owner), currentTick_(std::move(currentTick)) {}

    [[nodiscard]] std::int64_t tick() const override { return currentTick_ ? currentTick_() : 0; }

    server::plugins::PluginHandle after(std::int32_t delayTicks, std::function<void()> task) override;

    server::plugins::PluginHandle every(std::int32_t delayTicks, std::int32_t periodTicks,
                                        std::function<void()> task) override;

private:
    sched::Owner* owner_;
    std::function<std::int64_t()> currentTick_;
};

class DispatcherCommands final : public server::plugins::PluginCommands {
public:
    DispatcherCommands(sched::Owner& owner, command::CommandDispatcher& dispatcher,
                       std::function<const command::CommandSource*()> console)
        : owner_(&owner), dispatcher_(&dispatcher), console_(std::move(console)) {}

    server::plugins::PluginHandle
    registerCommand(std::string_view pluginId, const server::plugins::PluginCommandSpec& spec,
                    std::function<void(server::plugins::PluginCommandCall&)> run) override;

    bool runAsConsole(std::string_view command) override;

    void refresh();

private:
    struct Entry {
        std::string pluginId;
        server::plugins::PluginCommandSpec spec;
        std::function<void(server::plugins::PluginCommandCall&)> run;
        command::CommandNode* node = nullptr;
    };

    command::CommandNode* add(Entry& entry);

    sched::Owner* owner_;
    command::CommandDispatcher* dispatcher_;
    std::function<const command::CommandSource*()> console_;
    std::vector<std::unique_ptr<Entry>> entries_;
};

class RootPaths final : public server::plugins::PluginPaths {
public:
    explicit RootPaths(std::string root);

    [[nodiscard]] std::string packages() const override { return packages_; }

    [[nodiscard]] std::string folder(std::string_view pluginId) const override {
        return packages_ + "/" + std::string{pluginId};
    }

private:
    std::string packages_;
};

class SessionPackets final : public server::plugins::PluginPacketSender {
public:
    explicit SessionPackets(player::PlayerRegistry& registry) : registry_(&registry) {}

    bool send(server::plugins::PluginPlayerRef& player, std::string_view name,
              std::span<const std::uint8_t> bytes) override;

    [[nodiscard]] bool isKnown(std::string_view name) const override;

    [[nodiscard]] std::vector<std::string> names() const override;

private:
    player::PlayerRegistry* registry_;
};

class SessionChannels final : public server::plugins::PluginChannelSender {
public:
    explicit SessionChannels(player::PlayerRegistry& registry) : registry_(&registry) {}

    bool send(server::plugins::PluginPlayerRef& player, std::string_view channel,
              std::span<const std::uint8_t> bytes) override;

private:
    player::PlayerRegistry* registry_;
};

class ServerPluginServices final : public server::plugins::PluginServices {
public:
    struct Parts {
        server::plugins::PluginPlayers* players = nullptr;
        server::plugins::PluginWorlds* worlds = nullptr;
        server::plugins::PluginPlaceholders* placeholders = nullptr;
        server::plugins::PluginStorage* storage = nullptr;
        server::plugins::PluginMenus* menus = nullptr;
        server::plugins::PluginPacketSender* packets = nullptr;
        server::plugins::PluginChannelSender* channels = nullptr;
        std::function<sched::Owner*(const std::string& world, double x, double z)> ownerAt;
    };

    ServerPluginServices(sched::Owner& owner, command::CommandDispatcher& dispatcher,
                         std::function<std::int64_t()> currentTick,
                         std::function<const command::CommandSource*()> console, std::string root, Parts parts);

    [[nodiscard]] server::plugins::PluginTasks& tasks() override { return tasks_; }

    [[nodiscard]] server::plugins::PluginCommands& commands() override { return commands_; }

    [[nodiscard]] server::plugins::PluginPaths& paths() override { return paths_; }

    [[nodiscard]] server::plugins::PluginPlayers& players() override;
    [[nodiscard]] server::plugins::PluginWorlds& worlds() override;
    [[nodiscard]] server::plugins::PluginPlaceholders& placeholders() override;
    [[nodiscard]] server::plugins::PluginStorage& storage() override;
    [[nodiscard]] server::plugins::PluginMenus& menus() override;
    [[nodiscard]] server::plugins::PluginPacketSender& packets() override;
    [[nodiscard]] server::plugins::PluginChannelSender& channels() override;

    bool at(std::string_view world, double x, double z, std::function<void()> task) override;

    void refreshCommands() { commands_.refresh(); }

private:
    OwnerTasks tasks_;
    DispatcherCommands commands_;
    RootPaths paths_;
    Parts parts_;
};

} // namespace basalt::server::runtime::plugins

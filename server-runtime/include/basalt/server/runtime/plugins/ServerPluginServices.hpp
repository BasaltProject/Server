#pragma once

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>

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

/// Plugin work, on the owner's own queue: the same one the server's own tasks go through.
class OwnerTasks final : public server::plugins::PluginTasks {
public:
    OwnerTasks(sched::Owner& owner, std::function<std::int64_t()> currentTick)
        : owner_(&owner), currentTick_(std::move(currentTick)) {}

    [[nodiscard]] std::int64_t tick() const override { return currentTick_ ? currentTick_() : 0; }

    server::plugins::PluginHandle after(std::int32_t delayTicks, std::function<void()> task) override;

    /// Repeats by scheduling the next one as it runs.
    ///
    /// The owner has no repeating task of its own, and giving it one for this would mean a second
    /// kind of queue entry to cancel and to drain at shutdown. Rescheduling keeps one kind.
    server::plugins::PluginHandle every(std::int32_t delayTicks, std::int32_t periodTicks,
                                        std::function<void()> task) override;

private:
    sched::Owner* owner_;
    std::function<std::int64_t()> currentTick_;
};

/// Plugin commands, in the server's own tree, so they complete on the client like any other.
class DispatcherCommands final : public server::plugins::PluginCommands {
public:
    /// `console` is who a plugin's `run` acts as. Unset before there is one.
    DispatcherCommands(sched::Owner& owner, command::CommandDispatcher& dispatcher,
                       std::function<const command::CommandSource*()> console)
        : owner_(&owner), dispatcher_(&dispatcher), console_(std::move(console)) {}

    server::plugins::PluginHandle
    registerCommand(std::string_view pluginId, const server::plugins::PluginCommandSpec& spec,
                    std::function<void(server::plugins::PluginCommandCall&)> run) override;

    bool runAsConsole(std::string_view command) override;

    /// Registers everything again, after a rebuild threw the whole tree away.
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
    /// Held by pointer so an entry's address survives the list growing: a handle captures it.
    std::vector<std::unique_ptr<Entry>> entries_;
};

/// Where the packages and the folders being worked on live.
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

/// The real packet sender: finds the connection and writes the bytes with the right id in front.
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

/// The real channel sender: a custom payload down that player's own connection.
class SessionChannels final : public server::plugins::PluginChannelSender {
public:
    explicit SessionChannels(player::PlayerRegistry& registry) : registry_(&registry) {}

    bool send(server::plugins::PluginPlayerRef& player, std::string_view channel,
              std::span<const std::uint8_t> bytes) override;

private:
    player::PlayerRegistry* registry_;
};

/// The server, as much of it as the plugin host is allowed to borrow.
///
/// Everything a plugin runs is put on one owner: the global one, which is the only owner that exists
/// in both scheduler modes and is a single thread in both. That is what makes this stage's single
/// lane true rather than hopeful. When lanes follow regions, this is the seam that changes, and
/// nothing above it does.
class ServerPluginServices final : public server::plugins::PluginServices {
public:
    /// What the server hands over, each piece optional so a test can stand one up without a world.
    struct Parts {
        server::plugins::PluginPlayers* players = nullptr;
        server::plugins::PluginWorlds* worlds = nullptr;
        server::plugins::PluginPlaceholders* placeholders = nullptr;
        server::plugins::PluginStorage* storage = nullptr;
        server::plugins::PluginMenus* menus = nullptr;
        server::plugins::PluginPacketSender* packets = nullptr;
        server::plugins::PluginChannelSender* channels = nullptr;
        /// Who owns a position: the seam `at()` is built on. Unset answers "nobody", and it runs
        /// where it was called.
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

    /// Puts the plugin commands back after the tree was rebuilt.
    ///
    /// Rebuilding is how the command configuration is applied, and it starts from the commands
    /// registered in code, which plugin ones are not.
    void refreshCommands() { commands_.refresh(); }

private:
    OwnerTasks tasks_;
    DispatcherCommands commands_;
    RootPaths paths_;
    Parts parts_;
};

} // namespace basalt::server::runtime::plugins

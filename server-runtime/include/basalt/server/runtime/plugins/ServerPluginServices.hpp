#pragma once

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {
class CommandDispatcher;
class CommandNode;
class CommandSource;
class SuggestionContext;
} // namespace basalt::server::runtime::command

namespace basalt::wire::protocol::play {
struct CommandSuggestion;
}

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

    server::plugins::PluginHandle
    registerCompletingCommand(std::string_view pluginId, const server::plugins::PluginCommandSpec& spec,
                              std::function<void(server::plugins::PluginCommandCall&)> run, Completer complete) override;

    bool runAsConsole(std::string_view command) override;

    bool runAsPlayer(server::plugins::PluginPlayerRef& player, std::string_view command) override;

    void setPlayerRunner(std::function<bool(const std::string& uuid, const std::string& line)> runner) {
        playerRunner_ = std::move(runner);
    }

    void setPlayerReplier(std::function<void(const std::string& uuid, const wire::text::Component&)> replier) {
        playerReplier_ = std::move(replier);
    }

    void refresh();

private:
    struct Entry {
        std::string pluginId;
        server::plugins::PluginCommandSpec spec;
        std::function<void(server::plugins::PluginCommandCall&)> run;
        Completer complete;
        command::CommandNode* node = nullptr;
    };

    command::CommandNode* add(Entry& entry);
    std::vector<wire::protocol::play::CommandSuggestion> suggest(const Entry& entry,
                                                               const command::SuggestionContext& context);

    sched::Owner* owner_;
    command::CommandDispatcher* dispatcher_;
    std::function<const command::CommandSource*()> console_;
    std::function<bool(const std::string& uuid, const std::string& line)> playerRunner_;
    std::function<void(const std::string& uuid, const wire::text::Component&)> playerReplier_;
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

class NamedPackets : public server::plugins::PluginPacketSender {
public:
    [[nodiscard]] std::optional<server::plugins::PacketAddress> resolve(std::string_view name) const override;

    [[nodiscard]] std::optional<server::plugins::PacketScope> scopeOf(std::string_view name) const override;

    [[nodiscard]] std::string nameOf(const server::plugins::PacketAddress& address) const override;

    [[nodiscard]] std::vector<std::string> names() const override;
};

class SessionPackets final : public NamedPackets {
public:
    explicit SessionPackets(player::PlayerRegistry& registry) : registry_(&registry) {}

    bool send(server::plugins::PluginPlayerRef& player, const server::plugins::PacketAddress& address,
              std::span<const std::uint8_t> bytes, bool intercept) override;

    bool receive(server::plugins::PluginPlayerRef& player, const server::plugins::PacketAddress& address,
                 std::span<const std::uint8_t> bytes, bool intercept) override;

    [[nodiscard]] std::shared_ptr<server::plugins::PacketEndpoint>
    endpointOf(server::plugins::PluginPlayerRef& player) override;

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
        server::plugins::PluginInventories* inventories = nullptr;
        server::plugins::PluginAccess* access = nullptr;
        server::plugins::PluginScoreboards* scoreboards = nullptr;
        server::plugins::PluginBossBars* bossBars = nullptr;
        server::plugins::PluginAdvancements* advancements = nullptr;
        server::plugins::PluginServerControl* control = nullptr;
        server::plugins::PluginDatapacks* datapacks = nullptr;
        server::plugins::PluginMaps* maps = nullptr;
        server::plugins::PluginLoot* loot = nullptr;
        server::plugins::PluginSelectors* selectors = nullptr;
        server::plugins::PluginStructures* structures = nullptr;
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
    [[nodiscard]] server::plugins::PluginInventories& inventories() override;
    [[nodiscard]] server::plugins::PluginAccess& access() override;
    [[nodiscard]] server::plugins::PluginScoreboards& scoreboards() override;
    [[nodiscard]] server::plugins::PluginBossBars& bossBars() override;
    [[nodiscard]] server::plugins::PluginAdvancements& advancements() override;
    [[nodiscard]] server::plugins::PluginServerControl& serverControl() override;
    [[nodiscard]] server::plugins::PluginDatapacks& datapacks() override;
    [[nodiscard]] server::plugins::PluginMaps& maps() override;
    [[nodiscard]] server::plugins::PluginLoot& loot() override;
    [[nodiscard]] server::plugins::PluginSelectors& selectors() override;
    [[nodiscard]] server::plugins::PluginStructures& structures() override;

    bool at(std::string_view world, double x, double z, std::function<void()> task) override;

    void refreshCommands() { commands_.refresh(); }

    void setPlayerRunner(std::function<bool(const std::string& uuid, const std::string& line)> runner) {
        commands_.setPlayerRunner(std::move(runner));
    }

    void setPlayerReplier(std::function<void(const std::string& uuid, const wire::text::Component&)> replier) {
        commands_.setPlayerReplier(std::move(replier));
    }

private:
    OwnerTasks tasks_;
    DispatcherCommands commands_;
    RootPaths paths_;
    Parts parts_;
};

} // namespace basalt::server::runtime::plugins

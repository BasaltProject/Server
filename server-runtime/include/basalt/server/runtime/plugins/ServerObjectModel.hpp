#pragma once

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/plugins/ServerNpcs.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::config {
class GroupsConfig;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::storage {
class PlayerStore;
}

namespace basalt::wire::common::placeholder {
class PlaceholderRegistry;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

/// Where the object model reaches the rest of the server.
///
/// Callbacks rather than a back-reference to the whole thing: what these objects need is a handful
/// of lookups, and taking them one by one is what lets a test stand one up without a server.
struct ModelHooks {
    /// The world a name belongs to, or null when it is not loaded.
    std::function<world::LoadedWorld*(const std::string& name)> worldFor;
    /// Every loaded world, by name, and which one is the default.
    std::function<std::vector<world::LoadedWorld*>()> allWorlds;
    std::function<std::string()> defaultWorldName;
    /// The permission groups as they stand right now, which `/reload` can swap under a running
    /// server.
    std::function<const config::GroupsConfig&()> groups;
    /// Puts a mob into a world through the same path a natural spawn takes.
    std::function<entity::ServerEntity*(world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                        double y, double z)>
        spawnMob;
    /// Making a world, which only the server can do: it owns the folder, the seed and the scheduler.
    std::function<world::LoadedWorld*(const std::string& name, config::WorldDimension dimension, config::WorldType type,
                                      std::optional<std::int64_t> seed)>
        makeWorld;
    /// Where an NPC goes when a plugin makes one, and the ids it is given.
    NpcRegistry* npcs = nullptr;
    std::function<std::int32_t()> nextEntityId;
};

/// The server's own objects, seen through the plugin API's handles.
///
/// Handles, not copies: everything here reads the live thing when it is asked, and a handle to
/// somebody who left answers `online = false` rather than a stale name and a health of 20. The
/// writes go through the same paths a command would, so a plugin setting a gamemode fires the same
/// event and sends the same packets.
[[nodiscard]] std::shared_ptr<server::plugins::PluginPlayerRef>
playerRefFor(player::PlayerRegistry& registry, std::string uuid, std::string name, ModelHooks hooks);

/// Who is connected, and who was.
class RegistryPlayers final : public server::plugins::PluginPlayers {
public:
    RegistryPlayers(player::PlayerRegistry& registry, ModelHooks hooks)
        : registry_(&registry), hooks_(std::move(hooks)) {}

    [[nodiscard]] std::vector<std::shared_ptr<server::plugins::PluginPlayerRef>> online() const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginPlayerRef> byName(std::string_view name) const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginPlayerRef> byUuid(std::string_view uuid) const override;

private:
    player::PlayerRegistry* registry_;
    ModelHooks hooks_;
};

/// The worlds this server has.
class LoadedWorlds final : public server::plugins::PluginWorlds {
public:
    LoadedWorlds(ModelHooks hooks, std::function<std::vector<player::PlayerSession*>()> players)
        : hooks_(std::move(hooks)), players_(std::move(players)) {}

    [[nodiscard]] std::vector<std::shared_ptr<server::plugins::PluginWorldRef>> all() const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> byName(std::string_view name) const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> defaultWorld() const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> create(std::string_view name,
                                                                          std::string_view dimension,
                                                                          std::string_view type,
                                                                          std::optional<std::int64_t> seed) override;

private:
    ModelHooks hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
};

/// A plugin's placeholders, in the server's own registry.
///
/// Registering here is what makes `%arena_wins%` work in the MOTD and the sidebar without those
/// knowing a plugin exists. The prefix is the plugin's id, so two of them cannot collide by
/// accident.
class RegistryPlaceholders final : public server::plugins::PluginPlaceholders {
public:
    RegistryPlaceholders(wire::common::placeholder::PlaceholderRegistry& registry,
                         server::plugins::PluginPlayers& players,
                         std::function<std::vector<player::PlayerSession*>()> sessions)
        : registry_(&registry), players_(&players), sessions_(std::move(sessions)) {}

    server::plugins::PluginHandle registerPlaceholder(std::string_view pluginId, std::string_view name,
                                                      Value value) override;

    [[nodiscard]] std::string fill(std::string_view text, server::plugins::PluginPlayerRef* player) const override;

private:
    wire::common::placeholder::PlaceholderRegistry* registry_;
    server::plugins::PluginPlayers* players_;
    /// Rendering for a player needs the session the slot is typed on.
    std::function<std::vector<player::PlayerSession*>()> sessions_;
};

/// A plugin's data, in the player database, with the plugin's own id on every row.
///
/// The isolation is the shape of the calls rather than a check inside them: there is no way to ask
/// this for another plugin's namespace, because every query takes the caller's id and filters on it.
class DatabasePluginStorage final : public server::plugins::PluginStorage {
public:
    explicit DatabasePluginStorage(std::function<storage::PlayerStore*()> store) : store_(std::move(store)) {}

    [[nodiscard]] std::optional<std::string> playerValue(std::string_view pluginId, std::string_view uuid,
                                                         std::string_view key) override;

    void setPlayerValue(std::string_view pluginId, std::string_view uuid, std::string_view key,
                        const std::optional<std::string>& value) override;

    [[nodiscard]] std::map<std::string, std::string> playerValues(std::string_view pluginId,
                                                                  std::string_view uuid) override;

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> top(std::string_view pluginId, std::string_view key,
                                                                       std::int32_t limit) override;

    [[nodiscard]] std::map<std::string, std::int64_t> namespaces() override;

    std::int64_t purge(std::string_view pluginId) override;

private:
    std::function<storage::PlayerStore*()> store_;
};

/// Windows a plugin draws, opened on the session of whoever is looking at one.
///
/// All it does is find the connection: the window itself is the session's business, because that is
/// where the container machinery and the socket already are.
class SessionMenus final : public server::plugins::PluginMenus {
public:
    explicit SessionMenus(player::PlayerRegistry& registry) : registry_(&registry) {}

    server::plugins::PluginHandle open(server::plugins::PluginPlayerRef& player, std::string_view title,
                                       std::int32_t rows,
                                       const std::map<std::int32_t, server::plugins::PluginItem>& items,
                                       std::function<void(std::int32_t)> onClick) override;

    void close(server::plugins::PluginPlayerRef& player) override;

    void refresh(server::plugins::PluginPlayerRef& player,
                 const std::map<std::int32_t, server::plugins::PluginItem>& items) override;

private:
    [[nodiscard]] player::PlayerSession* sessionOf(const server::plugins::PluginPlayerRef& player) const;

    player::PlayerRegistry* registry_;
};

} // namespace basalt::server::runtime::plugins

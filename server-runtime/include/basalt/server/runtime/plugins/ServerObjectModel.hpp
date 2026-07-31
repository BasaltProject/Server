#pragma once

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/plugins/ServerNpcs.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::config {
class GroupsConfig;
}

namespace basalt::server::runtime::command {
class CommandEnvironment;
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
class PlayerProgress;
class RealRecipes;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {
class BlockChangeBroadcaster;
class BossBarView;
class MobActionSystem;
} // namespace basalt::server::runtime

namespace basalt::server::runtime::plugins {

struct ModelHooks {
    std::function<world::LoadedWorld*(const std::string& name)> worldFor;
    std::function<std::vector<world::LoadedWorld*>()> allWorlds;
    std::function<std::string()> defaultWorldName;
    std::function<const config::GroupsConfig&()> groups;
    std::function<entity::ServerEntity*(world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                        double y, double z)>
        spawnMob;
    std::function<world::LoadedWorld*(const std::string& name, config::WorldDimension dimension, config::WorldType type,
                                      std::optional<std::int64_t> seed)>
        makeWorld;
    std::function<bool(const std::string& name, bool save)> unloadWorld;
    std::function<const command::CommandEnvironment&()> environment;
    std::function<std::vector<std::string>(std::string_view registry)> tags;
    std::function<BlockChangeBroadcaster*()> blocks;
    NpcRegistry* npcs = nullptr;
    world::RealLootTables* lootTables = nullptr;
    const world::realBlockTags::TagMembers* blockTags = nullptr;
    const world::realBlockTags::TagMembers* itemTags = nullptr;
    std::function<bool(world::LoadedWorld& world, std::string_view action, std::int32_t value)> endFight;
    std::function<bool(world::LoadedWorld& world, std::string_view action, std::string_view text, std::int32_t value)>
        endFightBar;
    std::function<bool(world::LoadedWorld& world, std::string_view action, const std::array<std::int32_t, 3>& at,
                       const std::array<std::int32_t, 3>& source, std::int32_t value, std::string_view player)>
        sculk;
    std::function<std::string(world::LoadedWorld& world, std::string_view action,
                              const std::array<std::int32_t, 3>& at)>
        creakingHeart;
    std::function<bool(world::LoadedWorld& world, std::int32_t id, std::string_view action, std::string_view text,
                       std::int32_t value)>
        raidBar;
    std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)> endGatewayAt;
    std::function<std::int32_t()> nextEntityId;
    std::function<bool(const world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y,
                       double z, std::string_view cause)>
        spawnAllowed;
    std::function<entity::entityCombat::Hooks()> combatHooks;
    std::function<void(world::LoadedWorld& world, entity::ServerEntity& entity, const wire::common::Identifier& type,
                       std::int32_t causeId, bool died)>
        announceHit;
    std::function<bool(world::LoadedWorld& world, entity::ServerEntity& entity, const world::ActiveEffect& effect,
                       std::int32_t sourceId)>
        giveEffect;
    std::function<entity::ServerEntity*(world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                        double y, double z, float yaw, std::string_view reason)>
        spawnAs;
    std::function<void(world::LoadedWorld& world, double x, double y, double z, float power, bool breakBlocks,
                       std::int32_t sourceEntityId, bool fire)>
        explode;
    std::function<std::int32_t(world::LoadedWorld& world, player::PlayerSession& player, bool offHand)> reelIn;
    std::function<MobActionSystem*()> mobSystem;
    std::function<bool(std::int32_t entityId, const std::function<void(BossBarView&, player::PlayerRegistry&)>& use)>
        entityBar;
    std::function<world::PlayerProgress*()> progress;
    std::function<world::RealRecipes*()> recipes;
    std::function<storage::PlayerStore*()> store;
    std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)> randomTick;
};

[[nodiscard]] std::shared_ptr<server::plugins::PluginPlayerRef>
playerRefFor(player::PlayerRegistry& registry, std::string uuid, std::string name, ModelHooks hooks);

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

class LoadedWorlds final : public server::plugins::PluginWorlds {
public:
    LoadedWorlds(ModelHooks hooks, std::function<std::vector<player::PlayerSession*>()> players)
        : hooks_(std::make_shared<const ModelHooks>(std::move(hooks))), players_(std::move(players)) {}

    [[nodiscard]] std::vector<std::shared_ptr<server::plugins::PluginWorldRef>> all() const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> byName(std::string_view name) const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> defaultWorld() const override;

    [[nodiscard]] std::shared_ptr<server::plugins::PluginWorldRef> create(std::string_view name,
                                                                          std::string_view dimension,
                                                                          std::string_view type,
                                                                          std::optional<std::int64_t> seed) override;

    bool unload(std::string_view name, bool save) override;

    [[nodiscard]] std::int32_t stateId(std::string_view text) const override;
    [[nodiscard]] std::string stateText(std::int32_t stateId) const override;
    [[nodiscard]] std::vector<std::string> blockTable() const override;
    [[nodiscard]] std::vector<std::string> blockProperties(std::string_view block) const override;
    [[nodiscard]] std::int32_t stateFlags(std::int32_t stateId) const override;
    [[nodiscard]] std::vector<double> blockHardness(std::string_view block) const override;
    [[nodiscard]] std::vector<double> blockInfo(std::int32_t stateId) const override;
    [[nodiscard]] std::vector<std::int32_t> collisionBoxes(std::int32_t stateId) const override;
    [[nodiscard]] std::vector<std::string> gameRuleSpecs() const override;
    [[nodiscard]] std::int32_t gameEventFrequency(std::string_view event) const override;
    [[nodiscard]] std::string blockInstrument(std::int32_t stateId) const override;
    [[nodiscard]] std::int32_t nextEntityId() override;
    [[nodiscard]] std::string poiTypeOf(std::int32_t stateId) const override;
    [[nodiscard]] std::int32_t poiTickets(std::string_view type) const override;
    [[nodiscard]] std::vector<std::string> blockSounds(std::int32_t stateId) const override;
    [[nodiscard]] std::int32_t transformState(std::int32_t stateId, std::int32_t rotation,
                                              std::int32_t mirror) const override;
    [[nodiscard]] std::vector<double> toolFacts(std::string_view item, std::int32_t stateId) const override;
    [[nodiscard]] std::vector<std::string> blockEntityTypes() const override;
    [[nodiscard]] std::int32_t dataVersion() const override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>>
    fixNbt(std::string_view kind, std::span<const std::uint8_t> nbt, std::int32_t fromVersion) const override;
    [[nodiscard]] std::optional<std::string> fixName(std::string_view kind, std::string_view text,
                                                     std::int32_t fromVersion) const override;
    [[nodiscard]] std::vector<std::string> biomeIds() const override;
    [[nodiscard]] std::vector<std::string> itemIds() const override;
    [[nodiscard]] std::vector<std::string> tags(std::string_view registry) const override;
    [[nodiscard]] std::vector<std::string> entityTypeIds() const override;
    [[nodiscard]] std::vector<std::string> registryIds(std::string_view registry) const override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> registryData(std::string_view registry,
                                                                        std::string_view id) const override;
    [[nodiscard]] std::vector<std::string> entityDefaultAttributes(std::string_view type) const override;

private:
    std::shared_ptr<const ModelHooks> hooks_;
    std::function<std::vector<player::PlayerSession*>()> players_;
};

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
    std::function<std::vector<player::PlayerSession*>()> sessions_;
};

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

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> holderData(std::string_view kind,
                                                                      std::string_view id) override;

    void setHolderData(std::string_view kind, std::string_view id, std::span<const std::uint8_t> data) override;

private:
    std::function<storage::PlayerStore*()> store_;
};

class SessionMenus final : public server::plugins::PluginMenus {
public:
    explicit SessionMenus(player::PlayerRegistry& registry) : registry_(&registry) {}

    server::plugins::PluginHandle open(server::plugins::PluginPlayerRef& player, std::string_view title,
                                       std::int32_t rows,
                                       const std::map<std::int32_t, server::plugins::PluginItem>& items,
                                       std::function<void(std::int32_t)> onClick) override;

    void close(server::plugins::PluginPlayerRef& player) override;

    server::plugins::PluginHandle openForm(server::plugins::PluginPlayerRef& player,
                                           const server::plugins::PluginForm& form,
                                           server::plugins::PluginFormAnswer onAnswer) override;

    void refresh(server::plugins::PluginPlayerRef& player,
                 const std::map<std::int32_t, server::plugins::PluginItem>& items) override;

private:
    [[nodiscard]] player::PlayerSession* sessionOf(const server::plugins::PluginPlayerRef& player) const;

    player::PlayerRegistry* registry_;
};

} // namespace basalt::server::runtime::plugins

#pragma once

#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/world/DragonFight.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class AreaEffectCloudSystem;
class BlockChangeBroadcaster;
class CombatSystem;
class ExplosionSystem;

/// parity: the half of `EnderDragon.aiStep` and `tickDeath` that reaches outside the dragon.
class EndFightSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount)>
            awardExperience;
    };

    EndFightSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, CombatSystem& combat,
                   ExplosionSystem& explosions, AreaEffectCloudSystem& clouds, Options options);

    void tickDragon(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& dragon);

    /// parity: `EnderDragonFight.tick`.
    [[nodiscard]] bool killOutright(world::LoadedWorld& world, entity::ServerEntity& entity);

    void tickFight(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living);

    /// parity: `EnderDragonFight.onCrystalDestroyed` and `EnderDragon.onCrystalDestroyed`.
    void crystalDestroyed(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          const entity::ServerEntity& crystal, std::optional<std::int32_t> byPlayer);

    /// parity: `EnderDragonFight.tryRespawn`.
    void tryRespawn(world::LoadedWorld& world);

    /// parity: `EnderDragonFight.spawnNewGateway(BlockPos)`.
    bool spawnGatewayAt(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    /// What a plugin asks of the fight: respawn, stage, portal, gateway or crystals.
    bool control(world::LoadedWorld& world, std::string_view action, std::int32_t value);

private:
    [[nodiscard]] entity::enderDragon::DragonState& stateOf(entity::ServerEntity& dragon);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersIn(const world::LoadedWorld& world) const;

    /// parity: `DragonRespawnStage.tick`, one turn of the stage the respawn is at.
    void tickRespawn(world::LoadedWorld& world, world::DragonFight& fight,
                     const std::vector<player::PlayerSession*>& watchers,
                     const std::vector<entity::ServerEntity*>& crystals);

    void setRespawnStage(world::LoadedWorld& world, world::DragonFight& fight, world::RespawnStage stage);

    void abortRespawn(world::LoadedWorld& world, world::DragonFight& fight);

    /// parity: `EnderDragonFight.resetSpikeCrystals`.
    void resetSpikeCrystals(world::LoadedWorld& world);

    void levelEvent(const std::vector<player::PlayerSession*>& watchers, std::int32_t event, std::int32_t x,
                    std::int32_t y, std::int32_t z);

    void rebuildSpike(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      std::size_t index);

    [[nodiscard]] static world::DragonFight* fightOf(world::LoadedWorld& world, const entity::ServerEntity& dragon);

    void updatePlayers(world::LoadedWorld& world, world::DragonFight& fight,
                       const std::vector<player::PlayerSession*>& watchers);

    void showBar(world::DragonFight& fight, bool visible);

    void updateBar(world::DragonFight& fight, float progress);

    void scanState(world::LoadedWorld& world, world::DragonFight& fight,
                   const std::vector<entity::ServerEntity*>& living);

    void findOrCreateDragon(world::LoadedWorld& world, world::DragonFight& fight,
                            const std::vector<entity::ServerEntity*>& living);

    /// parity: `EnderDragonFight.tick`'s `removeTicketWithRadius(TicketType.DRAGON, ...)`.
    void releaseDragonTicket(world::LoadedWorld& world, world::DragonFight& fight);

    /// parity: `EnderDragonFight.createNewDragon`.
    void createNewDragon(world::LoadedWorld& world, world::DragonFight& fight);

    void updateCrystalCount(world::LoadedWorld& world, world::DragonFight& fight);

    /// parity: `EnderDragonFight.updateDragon`, each turn a dragon of this fight takes.
    void updateDragon(world::LoadedWorld& world, const entity::ServerEntity& dragon);

    /// parity: `EnderDragonFight.setDragonKilled`: the portal, a gateway and the egg.
    void dragonKilled(world::LoadedWorld& world, const entity::ServerEntity& dragon);

    void spawnExitPortal(world::LoadedWorld& world, world::DragonFight& fight, bool active);

    void spawnNewGateway(world::LoadedWorld& world, world::DragonFight& fight);

    void syncArea(world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                  std::int32_t maxZ);

    [[nodiscard]] entity::enderDragon::DragonWorld worldFor(world::LoadedWorld& world,
                                                            const std::vector<player::PlayerSession*>& watchers,
                                                            const entity::ServerEntity& dragon,
                                                            entity::enderDragon::DragonState& state);

    /// parity: `EnderDragon.checkCrystals`, the nearest crystal and the health it gives back.
    void checkCrystals(const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& dragon,
                       entity::enderDragon::DragonState& state);

    /// parity: `EnderDragon.knockBack` and `hurt`, the wings shoving and the head biting.
    void strike(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& dragon,
                entity::enderDragon::DragonState& state);

    /// parity: `EnderDragon.checkWalls`.
    bool checkWalls(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::enderDragon::DragonState& state, const std::array<double, 6>& box);

    /// parity: `EnderDragon.tickDeath`.
    void tickDeath(world::LoadedWorld& world, entity::ServerEntity& dragon, entity::enderDragon::DragonState& state);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    CombatSystem* combat_;
    ExplosionSystem* explosions_;
    AreaEffectCloudSystem* clouds_;
    Options options_;
};

} // namespace basalt::server::runtime

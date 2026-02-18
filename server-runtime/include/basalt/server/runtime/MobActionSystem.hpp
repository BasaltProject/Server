#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class CombatSystem;
class ExplosionSystem;
class NaturalSpawningSystem;
class RaidSystem;

/// What a goal is allowed to make the world do (swing, hit, breed, eat, ...), plus the rest of a
/// mob's own turn: fire and sunlight, the AI step itself, breeding and the loot it drops on death.
///
/// The goals cannot reach the server themselves, so implementing `MobActions` here is the whole of
/// their arm's length.
class MobActionSystem final : public entity::ai::MobActions {
public:
    /// What a plugin gets a say in. Callbacks rather than the plugin bus itself, and unset means
    /// nobody objects.
    struct Hooks {
        /// The hooks `entityCombat::hurt` itself consults, passed straight through.
        entity::entityCombat::Hooks combat;
        /// Whether a mob may catch fire in the sun.
        std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::int32_t fireTicks,
                           std::string_view cause)>
            combustAllowed;
        /// Whether these two animals may have a calf.
        std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& animal,
                           const entity::ServerEntity& partner)>
            breedAllowed;
    };

    /// The rolls a mob's turn needs, none of which is part of the level's own stream.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        std::function<double()> nextDouble;
    };

    /// Which worlds a mob might be in, since a goal only ever hands over the mob itself.
    using Worlds = std::function<std::vector<world::LoadedWorld*>()>;

    MobActionSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions,
                    NaturalSpawningSystem& spawning, CombatSystem& combat, RaidSystem& raids,
                    world::RealLootTables& lootTables, Worlds worlds, Hooks hooks, Options options);

    void swing(entity::ServerEntity& mob) override;

    void hurtPlayer(entity::ServerEntity& mob, std::int32_t playerId, float damage) override;

    void hurtMob(entity::ServerEntity& mob, entity::ServerEntity& target, float damage) override;

    void breed(entity::ServerEntity& mob, entity::ServerEntity& partner) override;

    void eatGrass(entity::ServerEntity& mob) override;

    void primeCreeper(entity::ServerEntity& mob) override;

    void explodeCreeper(entity::ServerEntity& mob) override;

    void setCharging(entity::ServerEntity& mob, bool charging) override;

    void shootFireball(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, bool large) override;

    void setBeamTarget(entity::ServerEntity& mob, std::optional<std::int32_t> targetId) override;

    void setCasting(entity::ServerEntity& mob, bool casting) override;

    void throwPotion(entity::ServerEntity& mob, double targetX, double targetY, double targetZ,
                     std::string_view potion) override;

    void castFangs(entity::ServerEntity& mob, double targetX, double targetY, double targetZ) override;

    void summonVexes(entity::ServerEntity& mob, std::int32_t count) override;

    void applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks,
                     std::int32_t amplifier) override;

    void setSleeping(entity::ServerEntity& mob, bool sleeping) override;

    void teleported(entity::ServerEntity& mob) override;

    void shootArrow(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, float power) override;

    /// parity: `SnowGolem.aiStep`, the layer of snow it leaves on every square it stands on.
    void trailSnow(world::LoadedWorld& world, const entity::ServerEntity& golem);

    /// The undead burning in the sun, and what burning costs anything that is on fire.
    void tickFire(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                  entity::ServerEntity& mob, random::RandomSource& random);

    /// parity: `LivingEntity.aiStep`'s travel half, which runs for a mob this build has no goals
    /// for as much as for one it does.
    void travelMob(world::LoadedWorld& world, entity::ServerEntity& mob, double speed);

    /// One mob's turn to decide and then move.
    void tickMobAi(world::LoadedWorld& world, entity::ServerEntity& mob,
                   const std::vector<entity::ai::PlayerView>& views, const std::vector<entity::ServerEntity*>& living,
                   random::RandomSource& random);

    /// The TRIGGERED rider forest a thunder horse becomes when somebody walks up to it.
    void activateSkeletonTrap(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    void tickVillageGolem(world::LoadedWorld& world, entity::ServerEntity& villager,
                          const std::vector<entity::ServerEntity*>& living, random::RandomSource& random);

    /// parity: `Entity.die` over `LootTable`, the drops a mob leaves where it fell.
    void dropDeathLoot(world::LoadedWorld& world, const entity::ServerEntity& entity);

    /// parity: `Animal.spawnChildFromBreeding`: the calf appears where its parent stands.
    void breed(world::LoadedWorld& world, entity::ServerEntity& animal, entity::ServerEntity& partner);

    /// Whether somebody is stood close enough to a cart to shove it. parity: two boxes touching.
    [[nodiscard]] static bool touchesCart(const entity::ServerEntity& cart, const player::PlayerSession& player);

private:
    /// The world this mob is in, which is the one thing a goal never hands over.
    [[nodiscard]] world::LoadedWorld* worldOf(const entity::ServerEntity& mob);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    /// One entity-data entry to everybody watching, which is how a mob's own flags travel.
    void sendEntityData(const world::LoadedWorld& world, std::int32_t entityId,
                        const wire::protocol::play::SetEntityDataPacket::Entry& entry);

    void playSound(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound,
                   float volume = 1.0F, float pitch = 1.0F);

    /// parity: `EatBlockGoal`: the grass in front becomes air and the block below becomes dirt.
    void eatGrassUnder(world::LoadedWorld& world, const entity::ServerEntity& mob);

    /// The light a mob is standing in, which is all the spiders need to know.
    [[nodiscard]] static std::int32_t lightAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position);

    [[nodiscard]] random::RandomSource freshRandom() const;

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    ExplosionSystem* explosions_;
    NaturalSpawningSystem* spawning_;
    CombatSystem* combat_;
    RaidSystem* raids_;
    world::RealLootTables* lootTables_;
    Worlds worlds_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime

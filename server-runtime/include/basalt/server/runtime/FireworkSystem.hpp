#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

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

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime {

class CombatSystem;

/// Firework rockets in flight: launched from a hand, a crossbow, a dispenser or a celebrating
/// villager, boosting whoever glides on one, and going off in stars that hurt what is near.
///
/// parity: `FireworkRocketEntity`.
class FireworkSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    FireworkSystem(player::PlayerRegistry& players, CombatSystem& combat, Options options);

    /// parity: `new FireworkRocketEntity(level, owner, x, y, z, stack)`, optionally shot at an angle.
    entity::ServerEntity* launch(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                                 std::optional<std::int32_t> ownerId, bool shotAtAngle = false);

    /// parity: a rocket shot at an angle through `Projectile.shoot`, a dispenser's or a crossbow's.
    entity::ServerEntity* shoot(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                                double dirX, double dirY, double dirZ, float power, float uncertainty,
                                std::optional<std::int32_t> ownerId);

    /// parity: `new FireworkRocketEntity(level, stack, stuckTo)`, the rocket a glider flies on.
    entity::ServerEntity* attach(world::LoadedWorld& world, wire::nbt::NbtCompound item, std::int32_t entityId,
                                 double x, double y, double z);

    /// parity: `FireworkRocketEntity.tick`.
    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& rocket);

private:
    /// parity: `explode`: the burst every client draws, the game event, the damage, and gone.
    void explode(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                 const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& rocket);

    /// parity: `dealExplosionDamage`.
    void dealExplosionDamage(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                             const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& rocket);

    player::PlayerRegistry* players_;
    CombatSystem* combat_;
    Options options_;
};

} // namespace basalt::server::runtime

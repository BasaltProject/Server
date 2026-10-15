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

class FireworkSystem {
public:
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    FireworkSystem(player::PlayerRegistry& players, CombatSystem& combat, Options options);

    /// parity: `new FireworkRocketEntity(level, owner, x, y, z, stack)`.
    entity::ServerEntity* launch(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                                 std::optional<std::int32_t> ownerId, bool shotAtAngle = false);

    entity::ServerEntity* shoot(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                                double dirX, double dirY, double dirZ, float power, float uncertainty,
                                std::optional<std::int32_t> ownerId);

    entity::ServerEntity* attach(world::LoadedWorld& world, wire::nbt::NbtCompound item, std::int32_t entityId,
                                 double x, double y, double z);

    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& rocket);

private:
    void explode(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                 const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& rocket);

    void dealExplosionDamage(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                             const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& rocket);

    player::PlayerRegistry* players_;
    CombatSystem* combat_;
    Options options_;
};

} // namespace basalt::server::runtime

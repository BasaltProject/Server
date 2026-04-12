#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobSteps.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>

#include <vector>

namespace basalt::server::runtime {

void MobActionSystem::propagateFall(world::LoadedWorld& world, entity::ServerEntity& vehicle, double fallen,
                                    const world::fallLanding::Landing& landing) {
    // parity: `Entity.propagateFallToPassengers`, the vehicle's fall and its block's modifier handed
    // to everyone on it, each through its own `causeFallDamage`.
    const std::vector<std::int32_t> riders = vehicle.passengerIds;
    for (const std::int32_t id : riders) {
        if (entity::ServerEntity* mob = world.state().entity(id); mob != nullptr) {
            const std::int32_t damage =
                world::fallLanding::damage(fallen, landing, mob->attributeWithModifiers("minecraft:safe_fall_distance"),
                                           mob->attributeWithModifiers("minecraft:fall_damage_multiplier")) -
                entity::mobSteps::fallDamageReduction(mob->type());
            if (damage <= 0) {
                continue;
            }
            movementSound(world, *mob, entity::mobSteps::fallSound(mob->type(), damage), 1.0F, 1.0F);
            entity::entityCombat::blame(*mob, std::nullopt, -1, wire::common::Identifier::vanilla("player"));
            const entity::entityCombat::Result result = entity::entityCombat::hurt(
                *mob, static_cast<float>(damage), std::nullopt, std::nullopt, hooks_.combat, "fall");
            if (result.landed) {
                combat_->announceEntityHit(watchersOf(world), *mob, world::damageTypes::kFall, -1, result.died);
            }
            continue;
        }
        for (player::PlayerSession* player : watchersOf(world)) {
            // parity: `Player.causeFallDamage`, nothing for a player who may fly, and the gamerule.
            if (player->entityId() != id || player->flying ||
                player->gamemode == wire::protocol::play::Gamemode::Creative ||
                player->gamemode == wire::protocol::play::Gamemode::Spectator ||
                !world.gameRules().get(world::gameRules::kFallDamage)) {
                continue;
            }
            const double effective = player::impulse::effectiveFallDistance(*player, fallen, player->y);
            const std::int32_t damage =
                world::fallLanding::damage(effective, landing, player::combat::kSafeFallDistance);
            if (damage > 0 && player->connection().hurt) {
                player::impulse::reset(*player);
                player->connection().hurt(static_cast<float>(damage), "fall", world::damageTypes::kFall, std::nullopt);
            }
        }
    }
}

} // namespace basalt::server::runtime

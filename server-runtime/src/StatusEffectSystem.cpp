#include <basalt/server/runtime/StatusEffectSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::runtime {
namespace {

// parity: each effect's own `shouldApplyEffectTickThisTick`, halved per level.
constexpr std::int32_t kRegenerationInterval = 50;
constexpr std::int32_t kPoisonInterval = 25;
constexpr std::int32_t kWitherInterval = 40;

/// parity: `shouldApplyEffectTickThisTick`, whose interval halves with every level.
[[nodiscard]] bool onInterval(std::int32_t ticks, std::int32_t base, std::int32_t amplifier) {
    const std::int32_t interval = amplifier >= 31 ? 0 : base >> amplifier;
    return interval <= 0 || ticks % interval == 0;
}

} // namespace

StatusEffectSystem::StatusEffectSystem(player::PlayerRegistry& players, TickOmens tickOmens)
    : players_(&players), tickOmens_(std::move(tickOmens)) {}

void StatusEffectSystem::tickEffects(world::LoadedWorld& world) const {
    for (player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name() || player->dead || player->effects.empty()) {
            continue;
        }
        // Copied out first: an effect that expires erases its own entry, and applying one can add
        // another, either of which would invalidate an iterator over the live map.
        std::vector<world::ActiveEffect> carried;
        carried.reserve(player->effects.size());
        for (const auto& [id, active] : player->effects) {
            carried.push_back(active);
        }
        for (const world::ActiveEffect& active : carried) {
            applyEffectTick(world, *player, active);
            if (active.infinite()) {
                continue;
            }
            const auto found = player->effects.find(active.effect);
            if (found == player->effects.end()) {
                continue;
            }
            found->second.remainingTicks--;
            if (found->second.remainingTicks <= 0) {
                player->effects.erase(found);
                if (player->connection().sendEffectRemoved) {
                    player->connection().sendEffectRemoved(active.effect);
                }
            }
        }
    }
}

void StatusEffectSystem::applyEffectTick(world::LoadedWorld& world, player::PlayerSession& player,
                                         const world::ActiveEffect& active) const {
    const std::int32_t ticks = active.remainingTicks;
    const player::PlayerConnection& connection = player.connection();
    if (active.effect == world::mobEffects::regeneration()) {
        if (onInterval(ticks, kRegenerationInterval, active.amplifier) && player.health() < player.maxHealth() &&
            connection.heal) {
            connection.heal(world.state().gameplay().mechanics.healthRegenAmount, "magic_regen");
        }
    } else if (active.effect == world::mobEffects::poison()) {
        // parity: poison never kills, so it stops at one point of health rather than at zero.
        if (onInterval(ticks, kPoisonInterval, active.amplifier) && player.health() > 1.0F && connection.hurt) {
            connection.hurt(world.state().gameplay().mechanics.poisonDegenerationAmount, "magic",
                            world::damageTypes::kMagic, std::nullopt);
        }
    } else if (active.effect == world::mobEffects::wither()) {
        if (onInterval(ticks, kWitherInterval, active.amplifier) && connection.hurt) {
            connection.hurt(world.state().gameplay().mechanics.witherDegenerationAmount, "wither",
                            world::damageTypes::kWither, std::nullopt);
        }
    } else if (active.effect == world::mobEffects::hunger()) {
        player.food.addExhaustion(world.state().gameplay().mechanics.hungerExhaustionAmount *
                                  static_cast<float>(active.amplifier + 1));
    } else if (active.effect == world::mobEffects::badOmen() || active.effect == world::mobEffects::raidOmen()) {
        if (tickOmens_) {
            tickOmens_(world, player, active);
        }
    }
}

} // namespace basalt::server::runtime

#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// A trident in a player's hand: raised, thrown or ridden on as a riptide.
///
/// parity: `TridentItem`.
namespace trident {

/// parity: `TridentItem.THROW_THRESHOLD_TIME`, `PROJECTILE_SHOOT_POWER` and `BASE_DAMAGE`.
inline constexpr std::int32_t kThrowThresholdTicks = 10;
inline constexpr float kShootPower = 2.5F;
inline constexpr float kBaseDamage = 8.0F;
/// parity: `startAutoSpinAttack(20, 8.0F, stack)`.
inline constexpr std::int32_t kSpinTicks = 20;

[[nodiscard]] bool isTrident(const storage::StoredItemStack& stack);

/// parity: `Entity.isInWaterOrRain`.
[[nodiscard]] bool inWaterOrRain(world::LoadedWorld& world, const PlayerSession& player);

/// parity: `EnchantmentHelper.getTridentSpinAttackStrength`, zero without riptide.
[[nodiscard]] float riptideStrength(const BreakContext& context, const storage::StoredItemStack& stack);

/// parity: `TridentItem.use`: refused when the next use would break it, or for a riptide trident
/// out of water and rain.
[[nodiscard]] bool canRaise(const BreakContext& context, world::LoadedWorld& world, const PlayerSession& player,
                            const storage::StoredItemStack& stack);

/// parity: `TridentItem.releaseUsing` after `heldTicks` of holding it up: thrown, or a riptide.
void release(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
             std::int32_t heldTicks);

} // namespace trident

} // namespace basalt::server::runtime::player

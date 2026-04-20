#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

namespace trident {

inline constexpr std::int32_t kThrowThresholdTicks = 10;
inline constexpr float kShootPower = 2.5F;
inline constexpr float kBaseDamage = 8.0F;
inline constexpr std::int32_t kSpinTicks = 20;

[[nodiscard]] bool isTrident(const storage::StoredItemStack& stack);

[[nodiscard]] bool inWaterOrRain(world::LoadedWorld& world, const PlayerSession& player);

/// parity: `EnchantmentHelper.getTridentSpinAttackStrength`, zero without riptide.
[[nodiscard]] float riptideStrength(const BreakContext& context, const storage::StoredItemStack& stack);

[[nodiscard]] bool canRaise(const BreakContext& context, world::LoadedWorld& world, const PlayerSession& player,
                            const storage::StoredItemStack& stack);

/// parity: `TridentItem.releaseUsing` after `heldTicks` of holding it up.
void release(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
             std::int32_t heldTicks);

} // namespace trident

} // namespace basalt::server::runtime::player

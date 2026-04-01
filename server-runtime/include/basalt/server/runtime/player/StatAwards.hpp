#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;
class PlayerRegistry;

/// Which custom statistic a thing the player does counts towards, where more than one place asks.
namespace statAwards {

/// parity: the statistic a block's `useWithoutItem` awards once its screen opens: a crafting table's
/// `interact_with_crafting_table`, a chest's `open_chest`, a hopper's `inspect_hopper`.
[[nodiscard]] std::optional<std::string_view> openStatOf(std::string_view blockPath);

/// parity: `ServerPlayer.checkRidingStatistics`, which vehicle a distance ridden counts towards.
[[nodiscard]] std::optional<std::string_view> ridingStatOf(const wire::common::Identifier& vehicle);

/// parity: `Player.awardStat(Stats.CUSTOM.get(name), amount)`.
void award(PlayerSession& player, std::string_view customName, std::int64_t amount = 1);

/// parity: `Stats.ITEM_USED` for that item.
void used(PlayerSession& player, std::string_view item);

/// parity: `ServerPlayer.drop` of a stack thrown from the hand or a screen: the item's count and
/// one `drop`.
void awardThrown(PlayerSession& player, std::string_view item, std::int32_t count);

/// parity: `EntityEvent.BREAK_MAINHAND` and `BREAK_OFFHAND`, which a client plays the break with.
inline constexpr std::int8_t kBreakMainHand = 47;
inline constexpr std::int8_t kBreakOffHand = 48;

/// parity: `LivingEntity.onEquippedItemBroken` and `ServerPlayer`'s statistic: the break everyone
/// in that world sees and hears, and one `broken` for the item.
void itemBroken(PlayerSession& player, std::string_view item, std::int8_t breakEvent, const PlayerRegistry* registry);

/// parity: `ServerPlayer.checkRidingStatistics`, one rounded step of what the player rode.
void awardRiding(PlayerSession& player, const wire::common::Identifier& vehicle, double dx, double dy, double dz);

} // namespace statAwards

} // namespace basalt::server::runtime::player

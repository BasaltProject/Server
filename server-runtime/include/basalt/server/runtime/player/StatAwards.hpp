#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;
class PlayerRegistry;

namespace statAwards {

[[nodiscard]] std::optional<std::string_view> openStatOf(std::string_view blockPath);

[[nodiscard]] std::optional<std::string_view> ridingStatOf(const wire::common::Identifier& vehicle);

void award(PlayerSession& player, std::string_view customName, std::int64_t amount = 1);

void used(PlayerSession& player, std::string_view item);

void awardThrown(PlayerSession& player, std::string_view item, std::int32_t count);

inline constexpr std::int8_t kBreakMainHand = 47;
inline constexpr std::int8_t kBreakOffHand = 48;

void itemBroken(PlayerSession& player, std::string_view item, std::int8_t breakEvent, const PlayerRegistry* registry);

void awardRiding(PlayerSession& player, const wire::common::Identifier& vehicle, double dx, double dy, double dz);

} // namespace statAwards

} // namespace basalt::server::runtime::player

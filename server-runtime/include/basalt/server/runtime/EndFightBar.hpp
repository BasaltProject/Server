#pragma once

#include <basalt/server/runtime/world/DragonFight.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::server::runtime::endFightBar {

inline constexpr std::int32_t kDarkenSky = 1;
inline constexpr std::int32_t kPlayMusic = 2;
inline constexpr std::int32_t kCreateFog = 4;

[[nodiscard]] wire::text::Component nameOf(const world::DragonFight& fight);

[[nodiscard]] wire::protocol::play::BossEventPacket::Add addOf(const world::DragonFight& fight);

} // namespace basalt::server::runtime::endFightBar

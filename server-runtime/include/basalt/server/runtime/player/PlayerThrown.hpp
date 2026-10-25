#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

[[nodiscard]] bool isThrowable(const wire::common::Identifier& item);

bool throwItemFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, std::int32_t slot);

using ShotGate =
    std::function<bool(const PlayerSession& player, const wire::common::Identifier& projectile, float power)>;

[[nodiscard]] float bowPowerOf(std::int32_t drawnTicks);

void shootBowFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                 std::int32_t drawnTicks, const ShotGate& allowed);

} // namespace basalt::server::runtime::player

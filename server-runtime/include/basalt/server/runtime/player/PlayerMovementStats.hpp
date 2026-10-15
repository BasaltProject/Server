#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {

namespace playerMovement {

struct Facts {
    bool riding = false;
    /// parity: `updateSwimming`, which is sprinting in water and not riding.
    bool swimming = false;
    bool eyeInWater = false;
    bool inWater = false;
    bool climbing = false;
    bool onGround = false;
    bool sprinting = false;
    bool crouching = false;
    bool fallFlying = false;
};

struct Counted {
    std::string key;
    std::int32_t centimetres = 0;
};

/// parity: `FLY_ONE_CM` only counts steps longer than this.
inline constexpr std::int32_t kFlyingThreshold = 25;

[[nodiscard]] bool isClimbable(std::int32_t stateId);

[[nodiscard]] std::optional<Counted> movementStat(double dx, double dy, double dz, const Facts& facts);

} // namespace playerMovement

} // namespace basalt::server::runtime::player

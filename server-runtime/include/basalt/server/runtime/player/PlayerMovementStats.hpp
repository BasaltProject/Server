#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {

/// Which distance counter a step belongs to.
///
/// parity: `ServerPlayer.checkMovementStatistics` and `checkRidingStatistics`, which pick exactly one
/// counter per step and round that step to centimetres on its own. Rounding per step matters: a
/// hundred steps of half a centimetre count as nothing in the jar, not as fifty.
namespace playerMovement {

/// What the player was doing when the step arrived.
struct Facts {
    /// Riding anything: the jar counts it on the vehicle's own counter, which needs the vehicle
    /// movement packet this build does not read yet, so a ridden step counts nowhere.
    bool riding = false;
    /// parity: `updateSwimming`, which is sprinting in water and not riding.
    bool swimming = false;
    /// Their eyes are in water, which is what tells walking under water from walking on it.
    bool eyeInWater = false;
    bool inWater = false;
    bool climbing = false;
    bool onGround = false;
    bool sprinting = false;
    bool crouching = false;
    /// parity: `isFallFlying`, gliding on an elytra.
    bool fallFlying = false;
};

/// A counter and how much to add to it.
struct Counted {
    std::string key;
    std::int32_t centimetres = 0;
};

/// parity: `FLY_ONE_CM` only counts steps longer than this, which keeps a standing player still.
inline constexpr std::int32_t kFlyingThreshold = 25;

/// Whether a block holds a player up as a ladder does. parity: `Entity.onClimbable`, which is also
/// what cancels a fall.
[[nodiscard]] bool isClimbable(std::int32_t stateId);

/// The counter this step belongs to, or nothing when the jar counts it nowhere.
[[nodiscard]] std::optional<Counted> movementStat(double dx, double dy, double dz, const Facts& facts);

} // namespace playerMovement

} // namespace basalt::server::runtime::player

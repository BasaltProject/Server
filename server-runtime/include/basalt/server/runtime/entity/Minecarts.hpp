#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// A minecart on a rail.
///
/// parity: `OldMinecartBehavior.moveAlongTrack`. A cart does not move freely: it is pinned to the
/// line between the rail's two exits and its speed is projected onto that line.
namespace minecarts {

/// parity: `getMaxSpeed`, 0.4 on land.
inline constexpr double kMaxSpeed = 0.4;

/// parity: `getSlowdownFactor`: a cart with somebody in it keeps far more of its speed.
inline constexpr double kSlowdown = 0.96;
inline constexpr double kRiddenSlowdown = 0.997;

/// The rail the cart is on: its own block, or the one under it. parity:
/// `getCurrentBlockPosOrRailBelow`.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> railUnder(const world::WorldState& world,
                                                                   const ServerEntity& cart);

/// Exactly where on the rail a point sits, height included, or nothing when there is no rail there.
///
/// parity: `OldMinecartBehavior.getPos`, whose sixteenth of a block of lift and its half-block step
/// on a slope are what the cart actually rides on.
[[nodiscard]] std::optional<std::array<double, 3>> railPosition(const world::WorldState& world, double x, double y,
                                                                double z);

/// One tick on a rail. False when there is no rail under the cart, which is when the caller lets it
/// fall instead.
bool moveAlongTrack(world::WorldState& world, ServerEntity& cart, double intentX = 0.0, double intentZ = 0.0,
                    bool ridden = false);

/// One tick off the rails.
///
/// parity: `AbstractMinecart.comeOffTrack`: the speed is capped, halved while it is on the ground,
/// and what is left of it is air drag.
void comeOffTrack(world::WorldState& world, ServerEntity& cart, double gravity);

/// parity: `MinecartTNT.primeFuse`'s own eighty ticks.
inline constexpr std::int32_t kTntCartFuse = 80;

/// parity: `MinecartTNT.explode`: four of power plus what the speed adds, and the speed counts up to
/// five however fast the cart was really going.
inline constexpr double kTntCartPower = 4.0;
inline constexpr double kTntCartSpeedCap = 5.0;

/// parity: the `activateMinecart` call at the end of a cart's turn, which every kind of cart reads
/// its own way: a hopper cart is **off** while the rail is powered, and a tnt cart lights its fuse.
/// It happens every turn the cart is on an activator rail, not only the turn it arrives.
///
/// `tntExplodes` is the game rule, which is what stops a fuse being lit at all.
void activateOnRail(world::WorldState& world, ServerEntity& cart, bool tntExplodes);

/// The direction the keys a rider is holding point, in the world.
///
/// parity: `ServerPlayer.getLastClientMoveIntent` over `Entity.getInputVector`: the keys make a
/// vector, it is normalised, and the player's own yaw turns it.
[[nodiscard]] std::array<double, 2> intentOf(bool forward, bool back, bool left, bool right, float yaw);

/// The shove a cart takes from somebody walking into it.
///
/// parity: `AbstractMinecart.push`, which pushes the cart away from whoever touched it at a
/// twentieth of the distance between them, and never harder than that.
void pushedBy(ServerEntity& cart, double fromX, double fromZ);

/// How fast the cart is going, for the callers that only care about that.
[[nodiscard]] double speedOf(const ServerEntity& cart);

} // namespace minecarts

} // namespace basalt::server::runtime::entity

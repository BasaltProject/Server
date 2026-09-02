#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// The end portal: arming a frame ring with eyes, and what happens to whoever steps into it.
///
/// parity: `EnderEyeItem.useOn` for the eye, `EndPortalFrameBlock.getOrCreatePortalShape` for the
/// ring, `EndPortalBlock.getPortalDestination` for the travel and `EndPlatformFeature` for the
/// obsidian slab everyone lands on. The ring is checked by hand rather than through a general block
/// pattern matcher: one pattern is not a matcher.
namespace endPortals {

/// parity: `ServerLevel.END_SPAWN_POINT`, the block everyone arrives at.
inline constexpr std::int32_t kSpawnX = 100;
inline constexpr std::int32_t kSpawnY = 50;
inline constexpr std::int32_t kSpawnZ = 0;

/// Where a portal leaves someone, in the world it takes them to.
///
/// Without the world the Kotlin's `PortalExit` carries: that is a `LoadedWorld`, which is not
/// ported, and the caller passed the destination in to begin with.
struct Exit {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    friend bool operator==(const Exit&, const Exit&) = default;
};

[[nodiscard]] bool isFrame(std::int32_t stateId);

[[nodiscard]] bool isEndPortal(std::int32_t stateId);

/// Whether this frame is still waiting for its eye.
[[nodiscard]] bool needsEye(std::int32_t stateId);

/// Puts an eye in the frame at these coordinates, and fills the ring if that was the last one.
///
/// Answers the blocks that changed, so the caller can tell the clients about them, and nothing at
/// all when there was no frame to arm.
[[nodiscard]] std::vector<BlockChange> placeEye(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Fills the ring these coordinates belong to, if all twelve frames are armed and facing the middle.
///
/// parity: the frames look inward, which is what the stronghold's own portal room builds and what
/// the pattern in `EndPortalFrameBlock` matches.
[[nodiscard]] std::vector<BlockChange> completeRing(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `EndPlatformFeature.createEndPlatform`: five by five of obsidian, three of air over it.
void createEndPlatform(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Where an end portal leaves someone, once the caller knows which world it takes them to.
///
/// parity: `EndPortalBlock.getPortalDestination`. Going out builds the platform and drops the
/// player on it; coming back uses the world's own spawn, which is what vanilla's respawn data
/// amounts to while there are no beds. `fromEnd` stands in for asking the source world which
/// dimension it is, and the spawn is passed in for the same reason: `LoadedWorld` is not ported.
[[nodiscard]] Exit exitFor(WorldState& to, bool fromEnd, double spawnX, double spawnY, double spawnZ);

} // namespace endPortals

} // namespace basalt::server::runtime::world

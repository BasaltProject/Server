#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `EnderEyeItem.useOn`.
namespace endPortals {

inline constexpr std::int32_t kSpawnX = 100;
inline constexpr std::int32_t kSpawnY = 50;
inline constexpr std::int32_t kSpawnZ = 0;

/// The destination is passed in by the caller, so this carries no `LoadedWorld`.
struct Exit {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    friend bool operator==(const Exit&, const Exit&) = default;
};

[[nodiscard]] bool isFrame(std::int32_t stateId);

[[nodiscard]] bool isEndPortal(std::int32_t stateId);

[[nodiscard]] bool needsEye(std::int32_t stateId);

[[nodiscard]] std::vector<BlockChange> placeEye(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: the frames look inward, as the stronghold's portal room builds them.
[[nodiscard]] std::vector<BlockChange> completeRing(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `EndPlatformFeature.createEndPlatform`.
void createEndPlatform(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `getPortalDestination`.
[[nodiscard]] Exit exitFor(WorldState& to, bool fromEnd, double spawnX, double spawnY, double spawnZ);

} // namespace endPortals

} // namespace basalt::server::runtime::world

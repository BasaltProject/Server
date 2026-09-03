#pragma once

#include <basalt/server/world/portal/PortalShape.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Where a portal leaves someone: the block they arrive on, and which world it is in.
struct PortalExit {
    LoadedWorld* world = nullptr;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// Lighting a Nether portal, and finding or building the one on the other side.
///
/// parity: `PortalShape` for the frame, `PortalForcer` for the exit, and `PortalIndex` in place of
/// the point-of-interest index vanilla searches, so a portal is found again even when its chunk is
/// long out of memory. What is still not vanilla is that neither will generate chunks to go looking:
/// the search answers from what the index knows.
namespace netherPortals {

/// parity: `Entity.getDimensionChangingDelay`, the 300 ticks that stop a portal loop.
inline constexpr std::int32_t kTravelCooldownTicks = 300;

/// Reads a world's blocks, generating nothing: what the frame search around a player needs.
[[nodiscard]] server::world::portal::BlockStateView view(LoadedWorld& world);

/// Lights the frame this block sits in, filling it with portal blocks.
///
/// parity: `BaseFireBlock.onPlace`, which is why this happens where the fire would go rather than
/// where the flint and steel was aimed.
[[nodiscard]] std::optional<server::world::portal::PortalShape> light(LoadedWorld& world, std::int32_t x,
                                                                      std::int32_t y, std::int32_t z);

/// The fire a flint and steel leaves when there is no frame to light.
bool placeFire(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Puts out any portal next to this block whose frame no longer holds it, and answers what went out.
///
/// parity: `NetherPortalBlock.updateShape`, which turns a portal block into air the moment its own
/// shape stops being complete. Vanilla lets that cascade one neighbour at a time through block
/// updates; this walks the connected blocks itself, which ends in the same place.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> extinguishAround(LoadedWorld& world, std::int32_t x,
                                                                        std::int32_t y, std::int32_t z);

/// Where someone standing in a portal comes out in the other world.
///
/// parity: `NetherPortalBlock.getPortalDestination`. The horizontal position scales by the ratio of
/// the two dimensions' coordinate scales (the 1:8), the vertical does not, and an existing portal
/// near the scaled position wins over building a new one.
[[nodiscard]] std::optional<PortalExit> exitFor(LoadedWorld& from, LoadedWorld& to, double x, double y, double z,
                                                server::world::portal::PortalAxis axis);

/// The nearest portal block the world's own index knows about.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findClosestPortal(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t radius);

/// The same search over the chunks in memory, which is what the index is checked against.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findClosestLoadedPortal(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t radius);

/// Builds a portal near this position and answers the block to arrive on.
///
/// parity: `PortalForcer.createPortal`, in its shape rather than its search: a four by five obsidian
/// frame around a two by three opening, on the first column with room for it, and if there is none,
/// at a clamped height on a platform of its own, which is how vanilla always manages to make one.
[[nodiscard]] std::array<std::int32_t, 3> createPortal(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, server::world::portal::PortalAxis axis);

/// The world's own vertical bounds, which a portal has to stay inside.
[[nodiscard]] std::int32_t minY(const LoadedWorld& world);

[[nodiscard]] std::int32_t maxY(const LoadedWorld& world);

} // namespace netherPortals

} // namespace basalt::server::runtime::world

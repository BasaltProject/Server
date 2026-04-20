#pragma once

#include <basalt/server/world/portal/PortalShape.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;

struct PortalExit {
    LoadedWorld* world = nullptr;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// parity: `PortalShape`.
namespace netherPortals {

inline constexpr std::int32_t kTravelCooldownTicks = 300;

[[nodiscard]] server::world::portal::BlockStateView view(LoadedWorld& world);

/// parity: `BaseFireBlock.onPlace`.
[[nodiscard]] std::optional<server::world::portal::PortalShape> light(LoadedWorld& world, std::int32_t x,
                                                                      std::int32_t y, std::int32_t z);

bool placeFire(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `NetherPortalBlock.updateShape`.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> extinguishAround(LoadedWorld& world, std::int32_t x,
                                                                        std::int32_t y, std::int32_t z);

/// parity: `getPortalDestination`.
[[nodiscard]] std::optional<PortalExit> exitFor(LoadedWorld& from, LoadedWorld& to, double x, double y, double z,
                                                server::world::portal::PortalAxis axis);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findClosestPortal(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t radius);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findClosestLoadedPortal(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t radius);

/// parity: `PortalForcer.createPortal`.
[[nodiscard]] std::array<std::int32_t, 3> createPortal(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, server::world::portal::PortalAxis axis);

[[nodiscard]] std::int32_t minY(const LoadedWorld& world);

[[nodiscard]] std::int32_t maxY(const LoadedWorld& world);

} // namespace netherPortals

} // namespace basalt::server::runtime::world

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <set>

namespace basalt::server::runtime::world::netherPortals {
namespace {

namespace portal = server::world::portal;
namespace reg = server::registry;

using portal::PortalAxis;
using portal::PortalShape;

/// parity: `PortalForcer`'s two radii: the Nether side is searched much closer in.
constexpr std::int32_t kNetherSearchRadius = 16;
constexpr std::int32_t kOverworldSearchRadius = 128;
constexpr std::int32_t kCreateSearchRadius = 16;
constexpr std::int32_t kSectionHeight = 16;

/// parity: `PortalForcer`'s own floor for a forced portal.
constexpr std::int32_t kForcedMinY = 70;

constexpr std::array<std::array<std::int32_t, 3>, 6> kNeighbours{
    {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

[[nodiscard]] std::int32_t obsidian() {
    static const std::int32_t state = reg::blocks::named("obsidian")->stateId;
    return state;
}

[[nodiscard]] std::int32_t fire() {
    static const std::int32_t state = reg::blocks::named("fire")->stateId;
    return state;
}

void reindex(LoadedWorld& world, const std::vector<std::array<std::int32_t, 3>>& blocks) {
    if (blocks.empty()) {
        return;
    }
    std::set<std::int64_t> chunks;
    for (const std::array<std::int32_t, 3>& block : blocks) {
        const std::int32_t chunkX = block[0] >> 4;
        const std::int32_t chunkZ = block[2] >> 4;
        const std::int64_t key = (static_cast<std::int64_t>(chunkX) << 32) | static_cast<std::uint32_t>(chunkZ);
        if (!chunks.insert(key).second) {
            continue;
        }
        if (const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ); chunk != nullptr) {
            world.portals().scan(*chunk);
        }
    }
}

[[nodiscard]] std::vector<std::array<std::int32_t, 3>> clearConnected(LoadedWorld& world, std::int32_t x,
                                                                      std::int32_t y, std::int32_t z) {
    std::vector<std::array<std::int32_t, 3>> cleared;
    std::deque<std::array<std::int32_t, 3>> pending{{x, y, z}};
    while (!pending.empty()) {
        const std::array<std::int32_t, 3> block = pending.front();
        pending.pop_front();
        const std::optional<std::int32_t> state = world.state().blockState(block[0], block[1], block[2]);
        if (!state.has_value() || !PortalShape::isPortal(*state)) {
            continue;
        }
        static_cast<void>(world.state().setBlockState(block[0], block[1], block[2], reg::blocks::air().stateId));
        cleared.push_back(block);
        for (const std::array<std::int32_t, 3>& side : kNeighbours) {
            pending.push_back({block[0] + side[0], block[1] + side[1], block[2] + side[2]});
        }
    }
    return cleared;
}

[[nodiscard]] std::array<std::int32_t, 2> step(PortalAxis axis) { return {portal::rightX(axis), portal::rightZ(axis)}; }

/// parity: `PortalForcer.canHostFrame`.
[[nodiscard]] bool canHostFrame(LoadedWorld& world, const std::array<std::int32_t, 3>& spot, PortalAxis axis) {
    const auto [stepX, stepZ] = step(axis);
    for (std::int32_t column = -1; column < 3; column++) {
        for (std::int32_t row = -1; row < 4; row++) {
            const std::int32_t blockX = spot[0] + stepX * column;
            const std::int32_t blockY = spot[1] + row;
            const std::int32_t blockZ = spot[2] + stepZ * column;
            if (blockY < minY(world) || blockY > maxY(world)) {
                return false;
            }
            const std::optional<std::int32_t> state = world.state().blockState(blockX, blockY, blockZ);
            if (!state.has_value()) {
                return false;
            }
            if (row < 0 && reg::blocks::isAir(*state)) {
                return false;
            }
            if (row >= 0 && !reg::blocks::isAir(*state)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> findFrameSpot(LoadedWorld& world, std::int32_t x,
                                                                       std::int32_t y, std::int32_t z, PortalAxis axis,
                                                                       std::int32_t ceiling) {
    std::optional<std::array<std::int32_t, 3>> best;
    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
    for (std::int32_t offsetX = -kCreateSearchRadius; offsetX <= kCreateSearchRadius; offsetX++) {
        for (std::int32_t offsetZ = -kCreateSearchRadius; offsetZ <= kCreateSearchRadius; offsetZ++) {
            const std::int32_t columnX = x + offsetX;
            const std::int32_t columnZ = z + offsetZ;
            const server::world::Chunk& chunk = world.state().getOrGenerateChunk(columnX >> 4, columnZ >> 4);
            const std::int32_t surface = std::min(ceiling - 5, chunk.heightAt(columnX & 15, columnZ & 15));
            if (surface <= minY(world)) {
                continue;
            }
            const std::array<std::int32_t, 3> candidate{columnX, surface + 1, columnZ};
            if (!canHostFrame(world, candidate, axis)) {
                continue;
            }
            const auto dx = static_cast<std::int64_t>(offsetX);
            const auto dz = static_cast<std::int64_t>(offsetZ);
            const auto dy = static_cast<std::int64_t>(candidate[1] - y);
            const std::int64_t distance = dx * dx + dz * dz + dy * dy;
            if (distance >= bestDistance) {
                continue;
            }
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

/// parity: `PortalForcer`'s last resort, which clamps the height and lays its own floor.
[[nodiscard]] std::array<std::int32_t, 3> forceFrameSpot(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z, PortalAxis axis, std::int32_t ceiling) {
    const std::array<std::int32_t, 3> spot{x, std::clamp(y, std::max(minY(world) + 1, kForcedMinY), ceiling - 9), z};
    const auto [stepX, stepZ] = step(axis);
    for (std::int32_t side = -1; side <= 1; side++) {
        for (std::int32_t column = 0; column < 2; column++) {
            for (std::int32_t row = -1; row < 3; row++) {
                const std::int32_t state = row < 0 ? obsidian() : reg::blocks::air().stateId;
                static_cast<void>(world.state().setBlockState(spot[0] + stepX * column + stepZ * side, spot[1] + row,
                                                              spot[2] + stepZ * column - stepX * side, state));
            }
        }
    }
    return spot;
}

void buildFrame(LoadedWorld& world, const std::array<std::int32_t, 3>& spot, PortalAxis axis) {
    const auto [stepX, stepZ] = step(axis);
    for (std::int32_t column = -1; column < 3; column++) {
        for (std::int32_t row = -1; row < 4; row++) {
            if (column != -1 && column != 2 && row != -1 && row != 3) {
                continue;
            }
            static_cast<void>(world.state().setBlockState(spot[0] + stepX * column, spot[1] + row,
                                                          spot[2] + stepZ * column, obsidian()));
        }
    }
    const std::int32_t portalState = PortalShape::portalState(axis);
    std::vector<std::array<std::int32_t, 3>> lit;
    for (std::int32_t column = 0; column < 2; column++) {
        for (std::int32_t row = 0; row < 3; row++) {
            const std::array<std::int32_t, 3> block{spot[0] + stepX * column, spot[1] + row, spot[2] + stepZ * column};
            static_cast<void>(world.state().setBlockState(block[0], block[1], block[2], portalState));
            lit.push_back(block);
        }
    }
    reindex(world, lit);
}

} // namespace

std::int32_t minY(const LoadedWorld& world) {
    return config::dimensionFacts(world.declaration().dimension).chunkHeight.minY();
}

std::int32_t maxY(const LoadedWorld& world) {
    const server::world::ChunkHeight height = config::dimensionFacts(world.declaration().dimension).chunkHeight;
    return height.minY() + height.height() - 1;
}

portal::BlockStateView view(LoadedWorld& world) {
    return [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.state().blockState(x, y, z).value_or(reg::blocks::air().stateId);
    };
}

std::optional<PortalShape> light(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<PortalShape> shape = PortalShape::findEmpty(view(world), x, y, z, minY(world));
    if (!shape.has_value()) {
        return std::nullopt;
    }
    const std::int32_t state = PortalShape::portalState(shape->axis());
    std::vector<std::array<std::int32_t, 3>> lit;
    for (const portal::PortalBlock& block : shape->interior()) {
        static_cast<void>(world.state().setBlockState(block.x, block.y, block.z, state));
        lit.push_back({block.x, block.y, block.z});
    }
    reindex(world, lit);
    return shape;
}

bool placeFire(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.state().setBlockState(x, y, z, fire());
}

std::vector<std::array<std::int32_t, 3>> extinguishAround(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                          std::int32_t z) {
    std::vector<std::array<std::int32_t, 3>> cleared;
    for (const std::array<std::int32_t, 3>& side : kNeighbours) {
        const std::int32_t blockX = x + side[0];
        const std::int32_t blockY = y + side[1];
        const std::int32_t blockZ = z + side[2];
        const std::optional<std::int32_t> state = world.state().blockState(blockX, blockY, blockZ);
        if (!state.has_value()) {
            continue;
        }
        const std::optional<PortalAxis> axis = PortalShape::axisOf(*state);
        if (!axis.has_value()) {
            continue;
        }
        const PortalShape shape = PortalShape::findAnyShape(view(world), blockX, blockY, blockZ, minY(world), *axis);
        if (shape.isComplete()) {
            continue;
        }
        const std::vector<std::array<std::int32_t, 3>> broken = clearConnected(world, blockX, blockY, blockZ);
        cleared.insert(cleared.end(), broken.begin(), broken.end());
    }
    reindex(world, cleared);
    return cleared;
}

std::optional<std::array<std::int32_t, 3>> findClosestPortal(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                             std::int32_t z, std::int32_t radius) {
    return world.portals().nearest(x, y, z, radius);
}

std::optional<std::array<std::int32_t, 3>> findClosestLoadedPortal(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                                   std::int32_t z, std::int32_t radius) {
    std::optional<std::array<std::int32_t, 3>> best;
    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
    for (std::int32_t chunkX = (x - radius) >> 4; chunkX <= ((x + radius) >> 4); chunkX++) {
        for (std::int32_t chunkZ = (z - radius) >> 4; chunkZ <= ((z + radius) >> 4); chunkZ++) {
            const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (std::size_t index = 0; index < chunk->sectionCount(); index++) {
                const server::world::ChunkSection& section = chunk->section(index);
                if (!section.mayContain([](std::int32_t state) { return PortalShape::isPortal(state); })) {
                    continue;
                }
                const std::int32_t sectionMinY =
                    chunk->verticalBounds().minY() + static_cast<std::int32_t>(index) * kSectionHeight;
                for (std::int32_t localY = 0; localY < kSectionHeight; localY++) {
                    for (std::int32_t localZ = 0; localZ < 16; localZ++) {
                        for (std::int32_t localX = 0; localX < 16; localX++) {
                            if (!PortalShape::isPortal(section.blockStateId(localX, localY, localZ))) {
                                continue;
                            }
                            const std::int32_t blockX = chunkX * 16 + localX;
                            const std::int32_t blockY = sectionMinY + localY;
                            const std::int32_t blockZ = chunkZ * 16 + localZ;
                            if (blockX < x - radius || blockX > x + radius || blockZ < z - radius ||
                                blockZ > z + radius) {
                                continue;
                            }
                            const auto dx = static_cast<std::int64_t>(blockX - x);
                            const auto dy = static_cast<std::int64_t>(blockY - y);
                            const auto dz = static_cast<std::int64_t>(blockZ - z);
                            const std::int64_t distance = dx * dx + dy * dy + dz * dz;
                            if (distance >= bestDistance) {
                                continue;
                            }
                            bestDistance = distance;
                            best = std::array<std::int32_t, 3>{blockX, blockY, blockZ};
                        }
                    }
                }
            }
        }
    }
    return best;
}

std::array<std::int32_t, 3> createPortal(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                         PortalAxis axis) {
    const server::world::ChunkHeight height = config::dimensionFacts(world.declaration().dimension).chunkHeight;
    const std::int32_t ceiling = std::min(maxY(world), height.minY() + height.height() - 1);
    const std::array<std::int32_t, 3> spot =
        findFrameSpot(world, x, y, z, axis, ceiling).value_or(forceFrameSpot(world, x, y, z, axis, ceiling));
    buildFrame(world, spot, axis);
    return spot;
}

std::optional<PortalExit> exitFor(LoadedWorld& from, LoadedWorld& to, double x, double y, double z, PortalAxis axis) {
    const double scale = config::dimensionFacts(from.declaration().dimension).coordinateScale /
                         config::dimensionFacts(to.declaration().dimension).coordinateScale;
    const auto targetX = static_cast<std::int32_t>(std::floor(x * scale));
    const auto targetZ = static_cast<std::int32_t>(std::floor(z * scale));
    const std::int32_t targetY = std::clamp(static_cast<std::int32_t>(y), minY(to) + 1, maxY(to) - 3);
    const bool toNether = to.declaration().dimension == config::WorldDimension::Nether;
    const std::int32_t radius = toNether ? kNetherSearchRadius : kOverworldSearchRadius;

    const std::optional<std::array<std::int32_t, 3>> existing =
        findClosestPortal(to, targetX, targetY, targetZ, radius);
    const std::array<std::int32_t, 3> landing = existing.value_or(createPortal(to, targetX, targetY, targetZ, axis));
    return PortalExit{.world = &to, .x = landing[0] + 0.5, .y = static_cast<double>(landing[1]), .z = landing[2] + 0.5};
}

} // namespace basalt::server::runtime::world::netherPortals

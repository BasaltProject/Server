#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
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

/// parity: `Direction.get(POSITIVE, axis)`, which is what `PortalForcer.createPortal` builds along.
[[nodiscard]] std::array<std::int32_t, 2> step(PortalAxis axis) {
    return {axis == PortalAxis::X ? 1 : 0, axis == PortalAxis::Z ? 1 : 0};
}

[[nodiscard]] std::int32_t blockAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.state().getOrGenerateChunk(x >> 4, z >> 4).blockStateId(x & 15, y, z & 15);
}

/// parity: `PortalForcer.canPortalReplaceBlock`.
[[nodiscard]] bool canPortalReplace(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t state = blockAt(world, x, y, z);
    return reg::blockShapes::isReplaceable(state) && !reg::blockFluids::hasFluid(state);
}

/// parity: `PortalForcer.canHostFrame`.
[[nodiscard]] bool canHostFrame(LoadedWorld& world, const std::array<std::int32_t, 3>& origin, PortalAxis axis,
                                std::int32_t offset) {
    const auto [stepX, stepZ] = step(axis);
    // parity: `Direction.getClockWise`.
    const std::int32_t clockX = -stepZ;
    const std::int32_t clockZ = stepX;
    for (std::int32_t width = -1; width < 3; width++) {
        for (std::int32_t height = -1; height < 4; height++) {
            const std::int32_t x = origin[0] + (stepX * width) + (clockX * offset);
            const std::int32_t y = origin[1] + height;
            const std::int32_t z = origin[2] + (stepZ * width) + (clockZ * offset);
            if (y < minY(world) || y > maxY(world)) {
                return false;
            }
            if (height < 0 && !reg::blockShapes::isSolidState(blockAt(world, x, y, z))) {
                return false;
            }
            if (height >= 0 && !canPortalReplace(world, x, y, z)) {
                return false;
            }
        }
    }
    return true;
}

/// parity: `BlockPos.spiralAround(origin, radius, EAST, SOUTH)`, the order that breaks ties between equal spots.
[[nodiscard]] std::vector<std::array<std::int32_t, 2>> spiralAround(std::int32_t centerX, std::int32_t centerZ,
                                                                    std::int32_t radius) {
    constexpr std::array<std::array<std::int32_t, 2>, 4> kDirections{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
    std::vector<std::array<std::int32_t, 2>> columns;
    std::int32_t lastX = centerX;
    std::int32_t lastZ = centerZ + 1;
    const std::int32_t legs = 4 * radius;
    std::int32_t leg = -1;
    std::int32_t legSize = 0;
    std::int32_t legIndex = 0;
    while (true) {
        const std::array<std::int32_t, 2>& direction = kDirections[static_cast<std::size_t>((leg + 4) % 4)];
        lastX += direction[0];
        lastZ += direction[1];
        if (legIndex >= legSize) {
            if (leg >= legs) {
                break;
            }
            leg++;
            legIndex = 0;
            legSize = (leg / 2) + 1;
        }
        legIndex++;
        columns.push_back({lastX, lastZ});
    }
    return columns;
}

/// parity: the search half of `PortalForcer.createPortal`, which scans every column from the top down.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> findFrameSpot(LoadedWorld& world, std::int32_t x,
                                                                       std::int32_t y, std::int32_t z, PortalAxis axis,
                                                                       std::int32_t maxPlaceableY) {
    const auto [stepX, stepZ] = step(axis);
    const std::int32_t lowest = minY(world);
    std::optional<std::array<std::int32_t, 3>> full;
    std::int64_t fullDistance = -1;
    std::optional<std::array<std::int32_t, 3>> partial;
    std::int64_t partialDistance = -1;
    const auto distanceTo = [&](const std::array<std::int32_t, 3>& at) {
        const auto dx = static_cast<std::int64_t>(at[0] - x);
        const auto dy = static_cast<std::int64_t>(at[1] - y);
        const auto dz = static_cast<std::int64_t>(at[2] - z);
        return dx * dx + dy * dy + dz * dz;
    };
    for (const std::array<std::int32_t, 2>& column :
         spiralAround(x, z, world.settings().rules.environment.portalCreateRadius)) {
        const std::int32_t height = std::min(
            maxPlaceableY,
            world.state().getOrGenerateChunk(column[0] >> 4, column[1] >> 4).heightAt(column[0] & 15, column[1] & 15));
        if (!world.border().contains(column[0], column[1]) ||
            !world.border().contains(column[0] + stepX, column[1] + stepZ)) {
            continue;
        }
        const std::int32_t px = column[0] - stepX;
        const std::int32_t pz = column[1] - stepZ;
        for (std::int32_t scan = height; scan >= lowest; scan--) {
            if (!canPortalReplace(world, px, scan, pz)) {
                continue;
            }
            const std::int32_t firstEmpty = scan;
            std::int32_t floor = scan;
            while (floor > lowest && canPortalReplace(world, px, floor - 1, pz)) {
                floor--;
            }
            scan = floor;
            if (floor + 4 > maxPlaceableY) {
                continue;
            }
            const std::int32_t delta = firstEmpty - floor;
            if (delta > 0 && delta < 3) {
                continue;
            }
            const std::array<std::int32_t, 3> at{px, floor, pz};
            if (!canHostFrame(world, at, axis, 0)) {
                continue;
            }
            const std::int64_t distance = distanceTo(at);
            if (canHostFrame(world, at, axis, -1) && canHostFrame(world, at, axis, 1) &&
                (fullDistance == -1 || fullDistance > distance)) {
                fullDistance = distance;
                full = at;
            }
            if (fullDistance == -1 && (partialDistance == -1 || partialDistance > distance)) {
                partialDistance = distance;
                partial = at;
            }
        }
    }
    return full.has_value() ? full : partial;
}

/// parity: `PortalForcer`'s last resort, which clamps the height and lays its own floor.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> forceFrameSpot(LoadedWorld& world, std::int32_t x,
                                                                        std::int32_t y, std::int32_t z, PortalAxis axis,
                                                                        std::int32_t maxPlaceableY) {
    const std::int32_t lowest = std::max(minY(world) + 1, kForcedMinY);
    const std::int32_t highest = maxPlaceableY - 9;
    if (highest < lowest) {
        return std::nullopt;
    }
    const auto [stepX, stepZ] = step(axis);
    const std::array<std::int32_t, 3> spot{x - stepX, std::clamp(y, lowest, highest), z - stepZ};
    const std::int32_t clockX = -stepZ;
    const std::int32_t clockZ = stepX;
    for (std::int32_t box = -1; box < 2; box++) {
        for (std::int32_t width = 0; width < 2; width++) {
            for (std::int32_t row = -1; row < 3; row++) {
                const std::int32_t state = row < 0 ? obsidian() : reg::blocks::air().stateId;
                static_cast<void>(world.state().setBlockState(spot[0] + (stepX * width) + (clockX * box), spot[1] + row,
                                                              spot[2] + (stepZ * width) + (clockZ * box), state));
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
    if (!world.events().allowed(&server::plugins::pluginEventTypes::worldPortalCreate,
                                [&](server::plugins::PluginEvent& event) {
                                    std::string blocks;
                                    for (const portal::PortalBlock& block : shape->interior()) {
                                        blocks += (blocks.empty() ? "" : "\n") + std::to_string(block.x) + "," +
                                                  std::to_string(block.y) + "," + std::to_string(block.z);
                                    }
                                    event.set("world", world.name());
                                    event.set("reason", std::string{"fire"});
                                    event.set("blocks", std::move(blocks));
                                })) {
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

std::optional<std::array<std::int32_t, 3>> createPortal(LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z, PortalAxis axis) {
    const server::world::ChunkHeight height = config::dimensionFacts(world.declaration().dimension).chunkHeight;
    // parity: `DimensionType.logicalHeight`, which keeps the Nether's portals under its roof.
    const std::int32_t logical =
        world.declaration().dimension == config::WorldDimension::Nether ? 128 : height.height();
    const std::int32_t maxPlaceableY = std::min(maxY(world), height.minY() + logical - 1);
    std::optional<std::array<std::int32_t, 3>> spot = findFrameSpot(world, x, y, z, axis, maxPlaceableY);
    if (!spot.has_value()) {
        spot = forceFrameSpot(world, x, y, z, axis, maxPlaceableY);
    }
    if (!spot.has_value()) {
        return std::nullopt;
    }
    buildFrame(world, *spot, axis);
    return spot;
}

std::optional<PortalExit> exitFor(LoadedWorld& from, LoadedWorld& to, double x, double y, double z, PortalAxis axis) {
    const double scale = config::dimensionFacts(from.declaration().dimension).coordinateScale /
                         config::dimensionFacts(to.declaration().dimension).coordinateScale;
    const auto targetX = static_cast<std::int32_t>(std::floor(x * scale));
    const auto targetZ = static_cast<std::int32_t>(std::floor(z * scale));
    const std::int32_t targetY = std::clamp(static_cast<std::int32_t>(y), minY(to) + 1, maxY(to) - 3);
    const std::int32_t radius =
        std::max(1, static_cast<std::int32_t>(to.settings().rules.environment.portalSearchRadius /
                                              config::dimensionFacts(to.declaration().dimension).coordinateScale));

    const std::optional<std::array<std::int32_t, 3>> existing =
        findClosestPortal(to, targetX, targetY, targetZ, radius);
    const std::optional<std::array<std::int32_t, 3>> made =
        existing.has_value() ? existing : createPortal(to, targetX, targetY, targetZ, axis);
    if (!made.has_value()) {
        return std::nullopt;
    }
    const std::array<std::int32_t, 3> landing = *made;
    return PortalExit{.world = &to, .x = landing[0] + 0.5, .y = static_cast<double>(landing[1]), .z = landing[2] + 0.5};
}

} // namespace basalt::server::runtime::world::netherPortals

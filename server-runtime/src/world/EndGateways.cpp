#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/EndGateways.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/end/EndFeatureBlocks.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cmath>
#include <utility>

namespace basalt::server::runtime::world::endGateways {
namespace {

namespace endFeatures = server::world::terrain::decoration::end;
using server::world::Chunk;

/// parity: `TheEndGatewayBlockEntity.findExitPortalXZPosTentative`'s numbers.
constexpr double kTeleportDistance = 1024.0;
constexpr std::int32_t kChunkLimit = 16;
constexpr std::int32_t kGatewayAboveSurface = 10;
constexpr std::int32_t kIslandY = 75;
constexpr std::int32_t kSearchFromY = 30;
constexpr std::int32_t kExitSearch = 5;
constexpr std::int32_t kIslandSearch = 16;
constexpr std::int32_t kHorizontalLimit = 30000000;
constexpr std::int32_t kVerticalLimit = 20000000;

[[nodiscard]] Chunk& chunkAt(WorldState& world, std::int32_t x, std::int32_t z) {
    return world.getOrGenerateChunk(x >> 4, z >> 4);
}

/// parity: `ChunkAccess.getHighestFilledSectionIndex`.
[[nodiscard]] std::optional<std::size_t> highestFilledSection(const Chunk& chunk) {
    for (std::size_t index = chunk.sectionCount(); index-- > 0;) {
        if (chunk.section(index).nonAirBlockCount() > 0) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool isEmpty(WorldState& world, double x, double z) {
    return !highestFilledSection(
                world.getOrGenerateChunk(wire::common::mth::floor(x / 16.0), wire::common::mth::floor(z / 16.0)))
                .has_value();
}

/// parity: `TheEndGatewayBlockEntity.findTallestBlock`.
[[nodiscard]] BlockPos findTallestBlock(WorldState& world, const BlockPos& around, std::int32_t dist,
                                        bool allowBedrock) {
    std::optional<BlockPos> tallest;
    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    for (std::int32_t xd = -dist; xd <= dist; ++xd) {
        for (std::int32_t zd = -dist; zd <= dist; ++zd) {
            if (xd == 0 && zd == 0 && !allowBedrock) {
                continue;
            }
            const std::int32_t x = around[0] + xd;
            const std::int32_t z = around[2] + zd;
            const Chunk& chunk = chunkAt(world, x, z);
            const std::int32_t floor = tallest.has_value() ? (*tallest)[1] : chunk.verticalBounds().minY();
            for (std::int32_t y = chunk.verticalBounds().maxY(); y > floor; --y) {
                const std::int32_t state = chunk.blockStateId(x & 15, y, z & 15);
                if (registry::blockShapes::isFullCube(state) && (allowBedrock || state != bedrock)) {
                    tallest = BlockPos{x, y, z};
                    break;
                }
            }
        }
    }
    return tallest.value_or(around);
}

/// parity: `TheEndGatewayBlockEntity.findValidSpawnInChunk`, the end stone nearest the origin with
/// two open blocks above it. `BlockPos.betweenClosed` walks x fastest, then y, then z.
[[nodiscard]] std::optional<BlockPos> findValidSpawnInChunk(const Chunk& chunk) {
    const std::optional<std::size_t> highest = highestFilledSection(chunk);
    const std::int32_t sectionBottom =
        highest.has_value() ? (chunk.verticalBounds().minSection() + static_cast<std::int32_t>(*highest)) * 16
                            : chunk.verticalBounds().minY();
    const std::int32_t maxY = sectionBottom + 15;
    const std::int32_t endStone = registry::blocks::named("end_stone")->stateId;
    const auto open = [&chunk](std::int32_t x, std::int32_t y, std::int32_t z) {
        return y > chunk.verticalBounds().maxY() || !registry::blockShapes::isFullCube(chunk.blockStateId(x, y, z));
    };
    std::optional<BlockPos> closest;
    double closestDist = 0.0;
    for (std::int32_t z = 0; z < 16; ++z) {
        for (std::int32_t y = kSearchFromY; y <= maxY; ++y) {
            for (std::int32_t x = 0; x < 16; ++x) {
                if (chunk.blockStateId(x, y, z) != endStone || !open(x, y + 1, z) || !open(x, y + 2, z)) {
                    continue;
                }
                const std::int32_t worldX = (chunk.chunkX() * 16) + x;
                const std::int32_t worldZ = (chunk.chunkZ() * 16) + z;
                const double dx = worldX + 0.5;
                const double dy = y + 0.5;
                const double dz = worldZ + 0.5;
                const double dist = (dx * dx) + (dy * dy) + (dz * dz);
                if (!closest.has_value() || dist < closestDist) {
                    closest = BlockPos{worldX, y, worldZ};
                    closestDist = dist;
                }
            }
        }
    }
    return closest;
}

/// parity: `BlockPos.asLong`.
[[nodiscard]] std::int64_t packed(const BlockPos& pos) {
    constexpr std::uint64_t kHorizontalMask = (1ULL << 26U) - 1U;
    constexpr std::uint64_t kVerticalMask = (1ULL << 12U) - 1U;
    const auto x = static_cast<std::uint64_t>(static_cast<std::int64_t>(pos[0])) & kHorizontalMask;
    const auto y = static_cast<std::uint64_t>(static_cast<std::int64_t>(pos[1])) & kVerticalMask;
    const auto z = static_cast<std::uint64_t>(static_cast<std::int64_t>(pos[2])) & kHorizontalMask;
    return static_cast<std::int64_t>((x << 38U) | (z << 12U) | y);
}

void setBlock(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    static_cast<void>(chunkAt(world, x, z));
    static_cast<void>(world.setBlockState(x, y, z, stateId));
}

/// parity: `TheEndGatewayBlockEntity.findOrCreateValidTeleportPos`.
[[nodiscard]] BlockPos findOrCreateValidTeleportPos(WorldState& world, std::int32_t x, std::int32_t z) {
    const double length = std::sqrt((static_cast<double>(x) * x) + (static_cast<double>(z) * z));
    const double dirX = length < 1.0E-5 ? 0.0 : x / length;
    const double dirZ = length < 1.0E-5 ? 0.0 : z / length;
    double atX = dirX * kTeleportDistance;
    double atZ = dirZ * kTeleportDistance;
    for (std::int32_t limit = kChunkLimit; !isEmpty(world, atX, atZ) && limit-- > 0;) {
        atX += dirX * -16.0;
        atZ += dirZ * -16.0;
    }
    for (std::int32_t limit = kChunkLimit; isEmpty(world, atX, atZ) && limit-- > 0;) {
        atX += dirX * 16.0;
        atZ += dirZ * 16.0;
    }
    const Chunk& chunk =
        world.getOrGenerateChunk(wire::common::mth::floor(atX / 16.0), wire::common::mth::floor(atZ / 16.0));
    std::optional<BlockPos> found = findValidSpawnInChunk(chunk);
    if (!found.has_value()) {
        const BlockPos island{wire::common::mth::floor(atX + 0.5), kIslandY, wire::common::mth::floor(atZ + 0.5)};
        random::RandomSource random = random::RandomSource::legacy(packed(island));
        endFeatures::endIslandFeature::placeBlocks(
            random, island[0], island[1], island[2],
            [&world](std::int32_t bx, std::int32_t by, std::int32_t bz, std::int32_t state) {
                setBlock(world, bx, by, bz, state);
            });
        found = island;
    }
    return findTallestBlock(world, *found, kIslandSearch, true);
}

[[nodiscard]] bool inSpawnableBounds(const BlockPos& pos) {
    return pos[1] >= -kVerticalLimit && pos[1] < kVerticalLimit && pos[0] >= -kHorizontalLimit &&
           pos[2] >= -kHorizontalLimit && pos[0] < kHorizontalLimit && pos[2] < kHorizontalLimit;
}

} // namespace

bool isGateway(std::int32_t stateId) {
    static const std::int32_t kGateway = [] {
        const registry::BlockState* found = registry::blocks::named("end_gateway");
        return found == nullptr ? -1 : found->stateId;
    }();
    return stateId == kGateway;
}

void place(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::optional<BlockPos> exit,
           bool exact) {
    endFeatures::endGatewayFeature::placeBlocks(x, y, z,
                                                [&world](std::int32_t bx, std::int32_t by, std::int32_t bz,
                                                         std::int32_t state) { setBlock(world, bx, by, bz, state); });
    wire::nbt::NbtCompound data;
    data.putLong("Age", 0LL);
    if (exit.has_value()) {
        data.putIntArray("exit_portal", {(*exit)[0], (*exit)[1], (*exit)[2]});
    }
    if (exact) {
        data.putBoolean("ExactTeleport", true);
    }
    chunkAt(world, x, z)
        .setBlockEntity(server::world::ChunkBlockEntity{
            x & 15, y, z & 15, wire::common::Identifier::vanilla("end_gateway"), std::move(data)});
}

std::optional<std::array<double, 3>> destination(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 bool inEnd) {
    const server::world::ChunkBlockEntity* entry = chunkAt(world, x, z).blockEntityAt(x & 15, y, z & 15);
    if (entry == nullptr) {
        return std::nullopt;
    }
    std::optional<BlockPos> exit;
    if (const std::span<const std::int32_t> saved = entry->data().getIntArray("exit_portal"); saved.size() == 3) {
        const BlockPos read{saved[0], saved[1], saved[2]};
        if (inSpawnableBounds(read)) {
            exit = read;
        }
    }
    const bool exact = entry->data().getBoolean("ExactTeleport");
    if (!exit.has_value() && inEnd) {
        BlockPos far = findOrCreateValidTeleportPos(world, x, z);
        far[1] += kGatewayAboveSurface;
        place(world, far[0], far[1], far[2], BlockPos{x, y, z}, false);
        // The far chunks may have moved things round; look the entry up again.
        Chunk& chunk = chunkAt(world, x, z);
        if (const server::world::ChunkBlockEntity* again = chunk.blockEntityAt(x & 15, y, z & 15); again != nullptr) {
            wire::nbt::NbtCompound data = again->data();
            data.putIntArray("exit_portal", {far[0], far[1], far[2]});
            if (exact) {
                data.putBoolean("ExactTeleport", true);
            }
            chunk.setBlockEntity(again->withData(std::move(data)));
        }
        exit = far;
    }
    if (!exit.has_value()) {
        return std::nullopt;
    }
    BlockPos at = *exit;
    if (!exact) {
        at = findTallestBlock(world, {at[0], at[1] + 2, at[2]}, kExitSearch, false);
        at[1] += 1;
    }
    return std::array<double, 3>{at[0] + 0.5, static_cast<double>(at[1]), at[2] + 0.5};
}

} // namespace basalt::server::runtime::world::endGateways

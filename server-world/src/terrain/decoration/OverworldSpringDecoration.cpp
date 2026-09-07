#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldBiomePaths.hpp>
#include <basalt/server/world/terrain/decoration/OverworldSpringDecoration.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kFluidSpringsStep = 8;
constexpr std::int32_t kWaterIndex = 0;
constexpr std::int32_t kLavaIndex = 1;
constexpr std::int32_t kFrozenLavaIndex = 2;
constexpr std::int32_t kLavaHeightInner = 8;

constexpr std::array<std::array<std::int32_t, 3>, 5> kNeighbours = {
    {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}}};

using PathSet = std::set<std::string, std::less<>>;

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

const PathSet& normalValidBlocks() {
    static const PathSet paths = {"stone", "granite", "diorite", "andesite", "deepslate", "tuff", "calcite", "dirt"};
    return paths;
}

const PathSet& frozenValidBlocks() {
    static const PathSet paths = {"snow_block", "powder_snow", "packed_ice"};
    return paths;
}

const PathSet& waterValidBlocks() {
    static const PathSet paths = [] {
        PathSet built = normalValidBlocks();
        built.insert(frozenValidBlocks().begin(), frozenValidBlocks().end());
        return built;
    }();
    return paths;
}

const PathSet& frozenLavaBiomes() {
    static const PathSet paths = {"grove", "snowy_slopes", "frozen_peaks", "jagged_peaks"};
    return paths;
}

struct Spring {
    std::int32_t stateId;
    wire::common::Identifier fluid;
    const PathSet* validBlocks;
    const PathSet* biomes;

    [[nodiscard]] bool isValid(std::optional<std::int32_t> candidate) const {
        if (!candidate.has_value()) {
            return false;
        }
        return validBlocks->find(pathOf(*candidate)) != validBlocks->end();
    }
};

[[nodiscard]] const Spring& waterSpring() {
    static const Spring spring{registry::blocks::water().stateId, wire::common::Identifier::vanilla("water"),
                               &waterValidBlocks(), &overworldBiomePaths()};
    return spring;
}

[[nodiscard]] const Spring& lavaSpring() {
    static const Spring spring{registry::blocks::lava().stateId, wire::common::Identifier::vanilla("lava"),
                               &normalValidBlocks(), &overworldBiomePaths()};
    return spring;
}

[[nodiscard]] const Spring& frozenLavaSpring() {
    static const Spring spring{registry::blocks::lava().stateId, wire::common::Identifier::vanilla("lava"),
                               &frozenValidBlocks(), &frozenLavaBiomes()};
    return spring;
}

void placeSpring(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, const Spring& spring,
                 const ExternalStateReader& externalBlockState) {
    const ChunkHeight bounds = chunk.verticalBounds();
    // Outside the chunk with no reader this answers nothing rather than air: a spring cannot judge
    // its own neighbourhood without the terrain, and guessing air would open holes at every border.
    const auto stateAt = [&](std::int32_t worldX, std::int32_t worldY,
                             std::int32_t worldZ) -> std::optional<std::int32_t> {
        if (worldY < bounds.minY() || worldY > bounds.maxY()) {
            return registry::blocks::air().stateId;
        }
        if ((worldX >> 4) == chunk.chunkX() && (worldZ >> 4) == chunk.chunkZ()) {
            return chunk.blockStateId(worldX & 15, worldY, worldZ & 15);
        }
        if (externalBlockState) {
            return externalBlockState(worldX, worldY, worldZ);
        }
        return std::nullopt;
    };

    if (!spring.isValid(stateAt(x, y + 1, z))) {
        return;
    }
    if (!spring.isValid(stateAt(x, y - 1, z))) {
        return;
    }
    const std::optional<std::int32_t> current = stateAt(x, y, z);
    if (!current.has_value()) {
        return;
    }
    if (!registry::blocks::isAir(*current) && !spring.isValid(current)) {
        return;
    }

    std::int32_t rockCount = 0;
    std::int32_t holeCount = 0;
    for (const std::array<std::int32_t, 3>& offset : kNeighbours) {
        const std::optional<std::int32_t> neighbour = stateAt(x + offset[0], y + offset[1], z + offset[2]);
        if (spring.isValid(neighbour)) {
            ++rockCount;
        }
        if (neighbour.has_value() && registry::blocks::isAir(*neighbour)) {
            ++holeCount;
        }
    }
    // parity: exactly four solid sides and exactly one hole, which is what makes a spring a spring
    // rather than an open pool.
    if (rockCount != 4 || holeCount != 1) {
        return;
    }

    chunk.setBlockStateId(x & 15, y, z & 15, spring.stateId);
    chunk.scheduleFluidTick(ChunkScheduledTick{spring.fluid, x & 15, y, z & 15, 0, 0});
}

[[nodiscard]] std::int32_t nextInt(random::RandomSource& random, std::int32_t minInclusive, std::int32_t maxInclusive) {
    if (minInclusive >= maxInclusive) {
        return minInclusive;
    }
    return minInclusive + random.nextInt(maxInclusive - minInclusive + 1);
}

[[nodiscard]] std::int32_t uniformWaterHeight(random::RandomSource& random, ChunkHeight bounds) {
    return bounds.minY() + random.nextInt(192 - bounds.minY() + 1);
}

/// parity: `VeryBiasedToBottomInt`, three nested draws that pile the result near the floor.
[[nodiscard]] std::int32_t veryBiasedHeight(random::RandomSource& random, ChunkHeight bounds) {
    const std::int32_t upperInclusive = nextInt(random, bounds.minY() + kLavaHeightInner, bounds.maxY() - 8);
    const std::int32_t biasedUpperInclusive = nextInt(random, bounds.minY(), upperInclusive - 1);
    return nextInt(random, bounds.minY(), biasedUpperInclusive - 1 + kLavaHeightInner);
}

void placeAttempts(Chunk& chunk, random::RandomSource& random, const surface::SurfaceRuleEnvironment& environment,
                   const Spring& spring, std::int32_t count, bool biasedHeight,
                   const ExternalStateReader& externalBlockState) {
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t worldX = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t worldZ = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = biasedHeight ? veryBiasedHeight(random, bounds) : uniformWaterHeight(random, bounds);
        if (spring.biomes->find(biomePath(environment.biomeAt(worldX, y, worldZ))) == spring.biomes->end()) {
            continue;
        }
        placeSpring(chunk, worldX, y, worldZ, spring, externalBlockState);
    }
}

} // namespace

void overworldSpringDecoration::decorateFeatures(Chunk& chunk, std::int64_t seed,
                                                 const surface::SurfaceRuleEnvironment& environment,
                                                 std::int32_t firstIndex, std::int32_t lastIndex,
                                                 ExternalStateReader externalBlockState) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (std::int32_t globalIndex = firstIndex; globalIndex <= lastIndex; ++globalIndex) {
        random::setFeatureSeed(random, decorationSeed, globalIndex, kFluidSpringsStep);
        switch (globalIndex) {
        case kWaterIndex:
            placeAttempts(chunk, random, environment, waterSpring(), 25, false, externalBlockState);
            break;
        case kLavaIndex:
            placeAttempts(chunk, random, environment, lavaSpring(), 20, true, externalBlockState);
            break;
        case kFrozenLavaIndex:
            placeAttempts(chunk, random, environment, frozenLavaSpring(), 20, true, externalBlockState);
            break;
        default:
            break;
        }
    }
}

void overworldSpringDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                         const surface::SurfaceRuleEnvironment& environment,
                                         ExternalStateReader externalBlockState) {
    decorateFeatures(chunk, seed, environment, 0, kFrozenLavaIndex, std::move(externalBlockState));
}

const std::set<std::string, std::less<>>& overworldSpringDecoration::placedSpringIds() {
    static const std::set<std::string, std::less<>> ids = {"spring_water", "spring_lava", "spring_lava_frozen"};
    return ids;
}

bool overworldSpringDecoration::placeAt(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z,
                                        std::string_view placedFeature) {
    const Spring* spring = nullptr;
    if (placedFeature == "spring_water") {
        spring = &waterSpring();
    } else if (placedFeature == "spring_lava") {
        spring = &lavaSpring();
    } else if (placedFeature == "spring_lava_frozen") {
        spring = &frozenLavaSpring();
    } else {
        return false;
    }
    placeSpring(chunk, x, y, z, *spring, nullptr);
    return true;
}

} // namespace basalt::server::world::terrain::decoration

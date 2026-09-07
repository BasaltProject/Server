#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SurfacePatches.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kWaterlilyIndex = 65;
constexpr std::int32_t kLeafLitterIndex = 77;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

void placeWaterlilyCandidate(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                             ChunkHeight bounds) {
    const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                               origin.y + random.nextInt(4) - random.nextInt(4),
                               origin.z + random.nextInt(8) - random.nextInt(8)};
    if (target.y < bounds.minY() || target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
        return;
    }
    const std::int32_t support = region.get(DecorationPos{target.x, target.y - 1, target.z});
    const std::string supportPath = pathOf(support);
    if (!registry::blockFluids::hasWaterFluid(support) && supportPath != "ice" && supportPath != "frosted_ice") {
        return;
    }
    region.set(target, stateIdOf("lily_pad"));
}

// parity: the weighted provider has twelve unit-weight entries, amounts 1 through 3, each in the
// horizontal plane order north, east, south, west.
constexpr std::array<std::int32_t, 4> kHorizontalFacingStateOffsets = {0, 12, 4, 8};

[[nodiscard]] std::int32_t leafLitterStateId(std::int32_t choice) {
    const std::int32_t amountOffset = choice / 4;
    const std::int32_t facingOffset = kHorizontalFacingStateOffsets[static_cast<std::size_t>(choice % 4)];
    return stateIdOf("leaf_litter") + facingOffset + amountOffset;
}

[[nodiscard]] bool canPlaceLeafLitter(const PatchRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (position.y < bounds.minY() || position.y > bounds.maxY() || !registry::blocks::isAir(region.get(position))) {
        return false;
    }
    if (position.y == bounds.minY()) {
        return false;
    }
    return pathOf(region.get(DecorationPos{position.x, position.y - 1, position.z})) == "grass_block";
}

} // namespace

void waterlilyPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                        const surface::SurfaceRuleEnvironment& environment,
                                        ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    static const std::set<std::string, std::less<>> biomes = {"mangrove_swamp", "swamp"};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kWaterlilyIndex, kVegetalDecorationStep);
    const PlacementContext context{chunk};
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < 4; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::WorldSurface);
        if (biomes.find(biomePath(environment.biomeAt(x, y, z))) == biomes.end()) {
            continue;
        }
        const DecorationPos origin{x, y, z};
        for (std::int32_t candidate = 0; candidate < 10; ++candidate) {
            placeWaterlilyCandidate(region, random, origin, bounds);
        }
    }
}

void waterlilyPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    PatchRegion region{chunk, nullptr, nullptr};
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t candidate = 0; candidate < 10; ++candidate) {
        placeWaterlilyCandidate(region, random, origin, bounds);
    }
}

void leafLitterPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                         const surface::SurfaceRuleEnvironment& environment,
                                         ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kLeafLitterIndex, kVegetalDecorationStep);

    const PlacementContext context{chunk};
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < 2; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::WorldSurface);
        if (biomePath(environment.biomeAt(x, y, z)) != "dark_forest") {
            continue;
        }
        const DecorationPos origin{x, y, z};
        for (std::int32_t candidate = 0; candidate < 32; ++candidate) {
            const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                       origin.y + random.nextInt(4) - random.nextInt(4),
                                       origin.z + random.nextInt(8) - random.nextInt(8)};
            if (!canPlaceLeafLitter(region, target, bounds)) {
                continue;
            }
            region.set(target, leafLitterStateId(random.nextInt(12)));
        }
    }
}

} // namespace basalt::server::world::terrain::decoration

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/TallGrassDecoration.hpp>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kSavannaIndex = 11;
constexpr std::int32_t kPlainIndex = 29;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

const std::set<std::string, std::less<>>& supportsVegetation() {
    static const std::set<std::string, std::less<>> paths = {
        "dirt",        "coarse_dirt", "rooted_dirt", "mud",     "muddy_mangrove_roots", "moss_block", "pale_moss_block",
        "grass_block", "podzol",      "mycelium",    "farmland"};
    return paths;
}

const std::set<std::string, std::less<>>& savannaBiomes() {
    static const std::set<std::string, std::less<>> paths = {"savanna", "savanna_plateau"};
    return paths;
}

const std::set<std::string, std::less<>>& plainBiomes() {
    static const std::set<std::string, std::less<>> paths = {"plains",          "sunflower_plains", "meadow",
                                                             "cherry_grove",    "sulfur_caves",     "lush_caves",
                                                             "dripstone_caves", "deep_dark"};
    return paths;
}

[[nodiscard]] std::int32_t upperStateId() {
    static const std::int32_t upper = [] {
        const std::optional<std::int32_t> found =
            registry::blockStateFamilies::upperDoublePlantStateId(stateIdOf("tall_grass"));
        if (!found.has_value()) {
            throw std::runtime_error{"tall_grass has no upper half"};
        }
        return *found;
    }();
    return upper;
}

void placePatch(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin, ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < 96; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(8) - random.nextInt(8)};
        // The upper half needs its own block, so the top of the world is excluded rather than
        // clamped.
        if (target.y < bounds.minY() || target.y >= bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
            continue;
        }
        const DecorationPos above{target.x, target.y + 1, target.z};
        if (!registry::blocks::isAir(region.get(above))) {
            continue;
        }
        const std::string belowPath = pathOf(region.get(DecorationPos{target.x, target.y - 1, target.z}));
        if (supportsVegetation().find(belowPath) == supportsVegetation().end()) {
            continue;
        }
        region.set(target, stateIdOf("tall_grass"));
        region.set(above, upperStateId());
    }
}

[[nodiscard]] DecorationPos inSquareAtHeight(const PlacementContext& context, random::RandomSource& random) {
    const std::int32_t x = context.chunk().chunkX() * 16 + random.nextInt(16);
    const std::int32_t z = context.chunk().chunkZ() * 16 + random.nextInt(16);
    return DecorationPos{x, context.height(x & 15, z & 15), z};
}

void placeSavanna(const PlacementContext& context, PatchRegion& region, random::RandomSource& random,
                  const surface::SurfaceRuleEnvironment& environment, ChunkHeight bounds) {
    if (random.nextFloat() >= 1.0F / 5.0F) {
        return;
    }
    const DecorationPos origin = inSquareAtHeight(context, random);
    if (savannaBiomes().find(biomePath(environment.biomeAt(origin.x, origin.y, origin.z))) == savannaBiomes().end()) {
        return;
    }
    placePatch(region, random, origin, bounds);
}

void placePlain(const PlacementContext& context, PatchRegion& region, random::RandomSource& random,
                const surface::SurfaceRuleEnvironment& environment, ChunkHeight bounds) {
    const DecorationPos chunkOrigin{context.chunk().chunkX() * 16, 0, context.chunk().chunkZ() * 16};
    // parity: the noise placement yields either nothing or seven positions, and vanilla only asks
    // whether it yielded any: the count itself is fixed at seven.
    const bool anyNoise = !noiseThresholdCountPlacement(-0.8, 0, 7)(context, random, chunkOrigin).empty();
    const std::int32_t outerCount = anyNoise ? 7 : 0;
    for (std::int32_t outer = 0; outer < outerCount; ++outer) {
        if (random.nextFloat() < 1.0F / 32.0F) {
            const DecorationPos origin = inSquareAtHeight(context, random);
            if (plainBiomes().find(biomePath(environment.biomeAt(origin.x, origin.y, origin.z))) !=
                plainBiomes().end()) {
                placePatch(region, random, origin, bounds);
            }
        }
    }
}

} // namespace

void tallGrassDecoration::decorateFeature(std::int32_t index, Chunk& chunk, std::int64_t seed,
                                          const surface::SurfaceRuleEnvironment& environment,
                                          ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const PlacementContext context{chunk};
    random::setFeatureSeed(random, decorationSeed, index, kVegetalDecorationStep);
    const ChunkHeight bounds = chunk.verticalBounds();
    if (index == kSavannaIndex) {
        placeSavanna(context, region, random, environment, bounds);
    } else if (index == kPlainIndex) {
        placePlain(context, region, random, environment, bounds);
    } else {
        throw std::invalid_argument{"unknown tall-grass vegetal index " + std::to_string(index)};
    }
}

void tallGrassDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                   ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    decorateFeature(kSavannaIndex, chunk, seed, environment, externalBlockState, observer);
    decorateFeature(kPlainIndex, chunk, seed, environment, externalBlockState, observer);
}

void tallGrassDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    PatchRegion region{chunk, nullptr, nullptr};
    placePatch(region, random, origin, chunk.verticalBounds());
}

} // namespace basalt::server::world::terrain::decoration

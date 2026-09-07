#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GrassScatterPatches.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kPumpkinIndex = 82;

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

/// The shape both patches share: a scatter of candidates that only take on a grass block.
void scatterOnGrass(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                    std::int32_t attempts, std::int32_t stateId, ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < attempts; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(8) - random.nextInt(8)};
        if (target.y < bounds.minY() || target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
            continue;
        }
        if (target.y == bounds.minY()) {
            continue;
        }
        const wire::common::Identifier* below =
            registry::blockStateDefinitions::blockOf(region.get(DecorationPos{target.x, target.y - 1, target.z}));
        if (below == nullptr || below->path() != "grass_block") {
            continue;
        }
        region.set(target, stateId);
    }
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::int32_t rarity;
    std::vector<std::string> biomes;
};

const std::vector<PatchSpec>& berrySpecs() {
    static const std::vector<PatchSpec> built = {
        {"patch_berry_common", 94, 32, {"old_growth_pine_taiga", "old_growth_spruce_taiga", "taiga"}},
        {"patch_berry_rare", 95, 384, {"snowy_taiga"}}};
    return built;
}

[[nodiscard]] std::int32_t sweetBerryAge3StateId() { return stateIdOf("sweet_berry_bush") + 3; }

void decorateBerrySpecs(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                        const std::vector<const PatchSpec*>& chosen, ExternalStateReader externalBlockState,
                        BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const PlacementContext context{chunk};
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const ChunkHeight bounds = chunk.verticalBounds();
    for (const PatchSpec* spec : chosen) {
        random::setFeatureSeed(random, decorationSeed, spec->index, kVegetalDecorationStep);
        if (random.nextFloat() >= 1.0F / static_cast<float>(spec->rarity)) {
            continue;
        }
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::WorldSurface);
        if (std::ranges::find(spec->biomes, biomePath(environment.biomeAt(x, y, z))) == spec->biomes.end()) {
            continue;
        }
        scatterOnGrass(region, random, DecorationPos{x, y, z}, 96, sweetBerryAge3StateId(), bounds);
    }
}

const std::set<std::string, std::less<>>& pumpkinBiomes() {
    static const std::set<std::string, std::less<>> biomes = {"badlands",
                                                              "bamboo_jungle",
                                                              "beach",
                                                              "birch_forest",
                                                              "cold_ocean",
                                                              "dark_forest",
                                                              "deep_cold_ocean",
                                                              "deep_dark",
                                                              "deep_frozen_ocean",
                                                              "deep_lukewarm_ocean",
                                                              "deep_ocean",
                                                              "desert",
                                                              "dripstone_caves",
                                                              "eroded_badlands",
                                                              "flower_forest",
                                                              "forest",
                                                              "frozen_ocean",
                                                              "frozen_river",
                                                              "grove",
                                                              "ice_spikes",
                                                              "jungle",
                                                              "lukewarm_ocean",
                                                              "ocean",
                                                              "old_growth_birch_forest",
                                                              "old_growth_pine_taiga",
                                                              "old_growth_spruce_taiga",
                                                              "pale_garden",
                                                              "plains",
                                                              "river",
                                                              "savanna",
                                                              "savanna_plateau",
                                                              "snowy_beach",
                                                              "snowy_plains",
                                                              "snowy_slopes",
                                                              "snowy_taiga",
                                                              "sparse_jungle",
                                                              "stony_shore",
                                                              "sunflower_plains",
                                                              "swamp",
                                                              "taiga",
                                                              "warm_ocean",
                                                              "windswept_forest",
                                                              "windswept_gravelly_hills",
                                                              "windswept_hills",
                                                              "windswept_savanna",
                                                              "wooded_badlands"};
    return biomes;
}

} // namespace

void berryPatchDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                    ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(berrySpecs().size());
    for (const PatchSpec& spec : berrySpecs()) {
        chosen.push_back(&spec);
    }
    decorateBerrySpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void berryPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                           const surface::SurfaceRuleEnvironment& environment, std::string_view name,
                                           ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : berrySpecs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown berry patch '" + std::string{name} + "'"};
    }
    decorateBerrySpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void berryPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    PatchRegion region{chunk, nullptr, nullptr};
    scatterOnGrass(region, random, origin, 96, sweetBerryAge3StateId(), chunk.verticalBounds());
}

void pumpkinPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                      const surface::SurfaceRuleEnvironment& environment,
                                      ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kPumpkinIndex, kVegetalDecorationStep);
    if (random.nextFloat() >= 1.0F / 300.0F) {
        return;
    }
    const PlacementContext context{chunk};
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
    const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
    const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::MotionBlocking);
    if (pumpkinBiomes().find(biomePath(environment.biomeAt(x, y, z))) == pumpkinBiomes().end()) {
        return;
    }
    scatterOnGrass(region, random, DecorationPos{x, y, z}, 96, stateIdOf("pumpkin"), chunk.verticalBounds());
}

void pumpkinPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    PatchRegion region{chunk, nullptr, nullptr};
    scatterOnGrass(region, random, origin, 96, stateIdOf("pumpkin"), chunk.verticalBounds());
}

} // namespace basalt::server::world::terrain::decoration

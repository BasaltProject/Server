#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/MushroomPatchDecoration.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kMushroomIslandIndex = 79;

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

const std::vector<std::string>& oldGrowthBiomes() {
    static const std::vector<std::string> biomes = {"old_growth_pine_taiga", "old_growth_spruce_taiga"};
    return biomes;
}

const std::vector<std::string>& swampBiomes() {
    static const std::vector<std::string> biomes = {"swamp"};
    return biomes;
}

const std::vector<std::string>& normalBiomes() {
    static const std::vector<std::string> biomes = {"badlands",
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
                                                    "ice_spikes",
                                                    "jungle",
                                                    "lukewarm_ocean",
                                                    "ocean",
                                                    "old_growth_birch_forest",
                                                    "old_growth_pine_taiga",
                                                    "old_growth_spruce_taiga",
                                                    "plains",
                                                    "river",
                                                    "savanna",
                                                    "savanna_plateau",
                                                    "snowy_beach",
                                                    "snowy_plains",
                                                    "sparse_jungle",
                                                    "stony_shore",
                                                    "sunflower_plains",
                                                    "swamp",
                                                    "warm_ocean",
                                                    "windswept_forest",
                                                    "windswept_gravelly_hills",
                                                    "windswept_hills",
                                                    "windswept_savanna",
                                                    "wooded_badlands"};
    return biomes;
}

const std::vector<std::string>& taigaBiomes() {
    static const std::vector<std::string> biomes = {"mushroom_fields", "snowy_taiga", "taiga"};
    return biomes;
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::string_view block;
    /// Zero means the spec has no rarity filter at all, not a filter that always passes.
    std::int32_t rarity;
    std::int32_t outerCount;
    const std::vector<std::string>* biomes;
};

const std::vector<PatchSpec>& specs() {
    static const std::vector<PatchSpec> built = {
        {"brown_mushroom_old_growth", 63, "brown_mushroom", 4, 3, &oldGrowthBiomes()},
        {"red_mushroom_old_growth", 64, "red_mushroom", 171, 1, &oldGrowthBiomes()},
        {"brown_mushroom_swamp", 66, "brown_mushroom", 0, 2, &swampBiomes()},
        {"red_mushroom_swamp", 67, "red_mushroom", 64, 1, &swampBiomes()},
        {"brown_mushroom_normal", 73, "brown_mushroom", 256, 1, &normalBiomes()},
        {"red_mushroom_normal", 74, "red_mushroom", 512, 1, &normalBiomes()},
        {"brown_mushroom_taiga", 80, "brown_mushroom", 4, 1, &taigaBiomes()},
        {"red_mushroom_taiga", 81, "red_mushroom", 256, 1, &taigaBiomes()}};
    return built;
}

void placeCandidate(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                    std::int32_t stateId, ChunkHeight bounds) {
    const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                               origin.y + random.nextInt(4) - random.nextInt(4),
                               origin.z + random.nextInt(8) - random.nextInt(8)};
    if (target.y < bounds.minY() || target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
        return;
    }
    if (target.y == bounds.minY() ||
        !registry::blockShapes::isSolidRender(region.get(DecorationPos{target.x, target.y - 1, target.z}))) {
        return;
    }
    region.set(target, stateId);
}

void placeSpec(const PlacementContext& context, PatchRegion& region, random::RandomSource& random,
               const surface::SurfaceRuleEnvironment& environment, const PatchSpec& spec, ChunkHeight bounds) {
    for (std::int32_t outer = 0; outer < spec.outerCount; ++outer) {
        if (spec.rarity != 0 && random.nextFloat() >= 1.0F / static_cast<float>(spec.rarity)) {
            continue;
        }
        const std::int32_t x = context.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = context.chunk().chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::MotionBlocking);
        if (std::ranges::find(*spec.biomes, biomePath(environment.biomeAt(x, y, z))) == spec.biomes->end()) {
            continue;
        }
        const DecorationPos origin{x, y, z};
        const std::int32_t stateId = stateIdOf(spec.block);
        for (std::int32_t attempt = 0; attempt < 96; ++attempt) {
            placeCandidate(region, random, origin, stateId, bounds);
        }
    }
}

void decorateSpecs(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
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
        placeSpec(context, region, random, environment, *spec, bounds);
    }
}

[[nodiscard]] std::vector<const PatchSpec*> allSpecs() {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    return chosen;
}

} // namespace

void mushroomPatchDecoration::decorateBeforeMushroomIsland(Chunk& chunk, std::int64_t seed,
                                                           const surface::SurfaceRuleEnvironment& environment) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : specs()) {
        if (spec.index < kMushroomIslandIndex) {
            chosen.push_back(&spec);
        }
    }
    decorateSpecs(chunk, seed, environment, chosen, nullptr, nullptr);
}

void mushroomPatchDecoration::decorateAfterMushroomIsland(Chunk& chunk, std::int64_t seed,
                                                          const surface::SurfaceRuleEnvironment& environment) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : specs()) {
        if (spec.index > kMushroomIslandIndex) {
            chosen.push_back(&spec);
        }
    }
    decorateSpecs(chunk, seed, environment, chosen, nullptr, nullptr);
}

void mushroomPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                       const surface::SurfaceRuleEnvironment& environment,
                                       ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    decorateSpecs(chunk, seed, environment, allSpecs(), std::move(externalBlockState), std::move(observer));
}

void mushroomPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                              const surface::SurfaceRuleEnvironment& environment, std::string_view name,
                                              ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : specs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown mushroom patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

const std::set<std::string, std::less<>>& mushroomPatchDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = [] {
        std::set<std::string, std::less<>> built;
        for (const PatchSpec& spec : specs()) {
            built.emplace(spec.name);
        }
        return built;
    }();
    return ids;
}

bool mushroomPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                      std::string_view placedFeature) {
    const PatchSpec* found = nullptr;
    for (const PatchSpec& spec : specs()) {
        if (spec.name == placedFeature) {
            found = &spec;
            break;
        }
    }
    if (found == nullptr) {
        return false;
    }
    PatchRegion region{chunk, nullptr, nullptr};
    const std::int32_t stateId = stateIdOf(found->block);
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < 96; ++attempt) {
        placeCandidate(region, random, origin, stateId, bounds);
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

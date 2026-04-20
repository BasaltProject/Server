#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FireflyBushPatchDecoration.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kHorizontalOffsets = {
    {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

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

const std::vector<std::string>& defaultBiomes() {
    static const std::vector<std::string> biomes = {"badlands",
                                                    "bamboo_jungle",
                                                    "beach",
                                                    "birch_forest",
                                                    "cold_ocean",
                                                    "dark_forest",
                                                    "deep_cold_ocean",
                                                    "deep_frozen_ocean",
                                                    "deep_lukewarm_ocean",
                                                    "deep_ocean",
                                                    "eroded_badlands",
                                                    "flower_forest",
                                                    "forest",
                                                    "frozen_ocean",
                                                    "frozen_river",
                                                    "ice_spikes",
                                                    "jungle",
                                                    "lukewarm_ocean",
                                                    "mangrove_swamp",
                                                    "mushroom_fields",
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
                                                    "snowy_taiga",
                                                    "sparse_jungle",
                                                    "stony_shore",
                                                    "sunflower_plains",
                                                    "taiga",
                                                    "warm_ocean",
                                                    "windswept_forest",
                                                    "windswept_gravelly_hills",
                                                    "windswept_hills",
                                                    "windswept_savanna",
                                                    "wooded_badlands"};
    return biomes;
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    /// Empty means the spec has no rarity filter at all, which is not the same as one that passes.
    std::optional<std::int32_t> rarity;
    std::int32_t outerCount;
    HeightmapType heightmap;
    bool nearWater;
    const std::vector<std::string>* biomes;
};

const std::vector<PatchSpec>& specs() {
    static const std::vector<std::string> swamp = {"swamp"};
    static const std::vector<PatchSpec> built = {
        {"patch_firefly_bush_swamp", 88, 8, 1, HeightmapType::MotionBlocking, false, &swamp},
        {"patch_firefly_bush_near_water_swamp", 89, std::nullopt, 3, HeightmapType::MotionBlocking, true, &swamp},
        {"patch_firefly_bush_near_water", 92, std::nullopt, 2, HeightmapType::MotionBlockingNoLeaves, true,
         &defaultBiomes()}};
    return built;
}

[[nodiscard]] bool canBushSurvive(const PatchRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (position.y <= bounds.minY()) {
        return false;
    }
    const std::string belowPath = pathOf(region.get(DecorationPos{position.x, position.y - 1, position.z}));
    return supportsVegetation().find(belowPath) != supportsVegetation().end();
}

[[nodiscard]] bool isNearWaterOrigin(const PatchRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (!registry::blocks::isAir(region.get(position)) || !canBushSurvive(region, position, bounds)) {
        return false;
    }
    return std::ranges::any_of(kHorizontalOffsets, [&](const std::pair<std::int32_t, std::int32_t>& offset) {
        return registry::blockFluids::hasWaterFluid(
            region.get(DecorationPos{position.x + offset.first, position.y - 1, position.z + offset.second}));
    });
}

void placeCandidates(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                     ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(5) - random.nextInt(5),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(5) - random.nextInt(5)};
        if (!registry::blocks::isAir(region.get(target)) || !canBushSurvive(region, target, bounds)) {
            continue;
        }
        region.set(target, stateIdOf("firefly_bush"));
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
        // parity: the rarity filter sits outside the outer loop, unlike the mushroom patches.
        if (spec->rarity.has_value() && random.nextFloat() >= 1.0F / static_cast<float>(*spec->rarity)) {
            continue;
        }
        for (std::int32_t outer = 0; outer < spec->outerCount; ++outer) {
            const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
            const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
            const std::int32_t y = context.height(x & 15, z & 15, spec->heightmap);
            const DecorationPos origin{x, y, z};
            if (std::ranges::find(*spec->biomes, biomePath(environment.biomeAt(x, y, z))) == spec->biomes->end()) {
                continue;
            }
            if (spec->nearWater && !isNearWaterOrigin(region, origin, bounds)) {
                continue;
            }
            placeCandidates(region, random, origin, bounds);
        }
    }
}

} // namespace

void fireflyBushPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                          const surface::SurfaceRuleEnvironment& environment,
                                          ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void fireflyBushPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                                 const surface::SurfaceRuleEnvironment& environment,
                                                 std::string_view name, ExternalStateReader externalBlockState,
                                                 BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : specs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown firefly bush patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

} // namespace basalt::server::world::terrain::decoration

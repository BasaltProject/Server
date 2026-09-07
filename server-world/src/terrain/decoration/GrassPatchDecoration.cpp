#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GrassPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <algorithm>
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

enum class Provider : std::uint8_t { Grass, Taiga, Jungle };

[[nodiscard]] std::int32_t providerState(Provider provider, random::RandomSource& random) {
    switch (provider) {
    case Provider::Grass:
        return stateIdOf("short_grass");
    case Provider::Taiga:
        return random.nextInt(5) == 0 ? stateIdOf("short_grass") : stateIdOf("fern");
    case Provider::Jungle:
        break;
    }
    return random.nextInt(4) < 3 ? stateIdOf("short_grass") : stateIdOf("fern");
}

struct NoiseCount {
    double level;
    std::int32_t below;
    std::int32_t above;
};

struct GrassSpec {
    std::int32_t index;
    std::int32_t outerCount;
    std::int32_t innerCount;
    std::vector<std::string> biomes;
    Provider provider = Provider::Grass;
    std::optional<NoiseCount> noise;
    bool rejectPodzol = false;
};

const std::vector<GrassSpec>& specs() {
    static const std::vector<GrassSpec> built = {
        {.index = 11,
         .outerCount = 25,
         .innerCount = 32,
         .biomes = {"bamboo_jungle", "jungle", "sparse_jungle"},
         .provider = Provider::Jungle,
         .rejectPodzol = true},
        {.index = 12, .outerCount = 20, .innerCount = 32, .biomes = {"savanna", "savanna_plateau"}},
        {.index = 35, .outerCount = 0, .innerCount = 16, .biomes = {"meadow"}, .noise = NoiseCount{-0.8, 5, 10}},
        {.index = 54,
         .outerCount = 0,
         .innerCount = 32,
         .biomes = {"cherry_grove", "deep_dark", "dripstone_caves", "plains", "sunflower_plains"},
         .noise = NoiseCount{-0.8, 5, 10}},
        {.index = 58,
         .outerCount = 7,
         .innerCount = 32,
         .biomes = {"old_growth_pine_taiga", "old_growth_spruce_taiga"},
         .provider = Provider::Taiga},
        {.index = 59,
         .outerCount = 2,
         .innerCount = 32,
         .biomes = {"birch_forest", "dark_forest", "forest", "old_growth_birch_forest", "pale_garden"}},
        {.index = 60,
         .outerCount = 1,
         .innerCount = 32,
         .biomes = {"snowy_taiga", "taiga"},
         .provider = Provider::Taiga},
        {.index = 61, .outerCount = 5, .innerCount = 32, .biomes = {"mangrove_swamp", "swamp", "windswept_savanna"}},
        {.index = 68,
         .outerCount = 1,
         .innerCount = 32,
         .biomes = {"badlands",
                    "beach",
                    "cold_ocean",
                    "deep_cold_ocean",
                    "deep_frozen_ocean",
                    "deep_lukewarm_ocean",
                    "deep_ocean",
                    "desert",
                    "eroded_badlands",
                    "flower_forest",
                    "frozen_ocean",
                    "frozen_river",
                    "ice_spikes",
                    "lukewarm_ocean",
                    "ocean",
                    "river",
                    "snowy_beach",
                    "snowy_plains",
                    "stony_shore",
                    "warm_ocean",
                    "windswept_forest",
                    "windswept_gravelly_hills",
                    "windswept_hills",
                    "wooded_badlands"}}};
    return built;
}

[[nodiscard]] const GrassSpec* specFor(std::int32_t index) {
    for (const GrassSpec& spec : specs()) {
        if (spec.index == index) {
            return &spec;
        }
    }
    return nullptr;
}

void placePatch(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin, const GrassSpec& spec,
                ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < spec.innerCount; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(8) - random.nextInt(8)};
        if (target.y < bounds.minY() || target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
            continue;
        }
        // parity: the podzol rejection is tested before the provider draws, and the general support
        // test after it. A candidate on podzol consumes no value; one on plain stone consumes one.
        if (spec.rejectPodzol && pathOf(region.get(DecorationPos{target.x, target.y - 1, target.z})) == "podzol") {
            continue;
        }
        const std::int32_t stateId = providerState(spec.provider, random);
        if (target.y == bounds.minY()) {
            continue;
        }
        const std::string belowPath = pathOf(region.get(DecorationPos{target.x, target.y - 1, target.z}));
        if (supportsVegetation().find(belowPath) == supportsVegetation().end()) {
            continue;
        }
        region.set(target, stateId);
    }
}

} // namespace

void grassPatchDecoration::decorateFeature(std::int32_t index, Chunk& chunk, std::int64_t seed,
                                           const surface::SurfaceRuleEnvironment& environment,
                                           ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    const GrassSpec* spec = specFor(index);
    if (spec == nullptr) {
        throw std::invalid_argument{"unknown grass vegetal index " + std::to_string(index)};
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const PlacementContext context{chunk};
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    random::setFeatureSeed(random, decorationSeed, spec->index, kVegetalDecorationStep);

    const ChunkHeight bounds = chunk.verticalBounds();
    const DecorationPos chunkOrigin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
    std::int32_t outerCount = spec->outerCount;
    if (spec->noise.has_value()) {
        outerCount =
            static_cast<std::int32_t>(noiseThresholdCountPlacement(spec->noise->level, spec->noise->below,
                                                                   spec->noise->above)(context, random, chunkOrigin)
                                          .size());
    }
    for (std::int32_t outer = 0; outer < outerCount; ++outer) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{x, context.height(x & 15, z & 15, HeightmapType::WorldSurface), z};
        if (std::ranges::find(spec->biomes, biomePath(environment.biomeAt(origin.x, origin.y, origin.z))) !=
            spec->biomes.end()) {
            placePatch(region, random, origin, *spec, bounds);
        }
    }
}

void grassPatchDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                    ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    // parity: each spec re-seeds from the world seed rather than sharing one stream, so the loop
    // hands the readers down per spec instead of building a region once.
    for (const GrassSpec& spec : specs()) {
        decorateFeature(spec.index, chunk, seed, environment, externalBlockState, observer);
    }
}

const std::map<std::string, std::int32_t, std::less<>>& grassPatchDecoration::placedGrassIds() {
    static const std::map<std::string, std::int32_t, std::less<>> ids = {
        {"patch_grass_jungle", 11},
        {"patch_grass_savanna", 12},
        {"patch_grass_meadow", 35},
        {"patch_grass_plain", 54},
        {"patch_grass_taiga", 58},
        // The village feature of this name is a different placement over the same body.
        {"patch_taiga_grass", 58},
        {"patch_grass_forest", 59},
        {"patch_grass_taiga_2", 60},
        {"patch_grass_normal", 61},
        {"patch_grass_badlands", 68}};
    return ids;
}

bool grassPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                   std::string_view placedFeature) {
    const auto named = placedGrassIds().find(placedFeature);
    if (named == placedGrassIds().end()) {
        return false;
    }
    const GrassSpec* spec = specFor(named->second);
    if (spec == nullptr) {
        return false;
    }
    PatchRegion region{chunk, nullptr, nullptr};
    placePatch(region, random, origin, *spec, chunk.verticalBounds());
    return true;
}

} // namespace basalt::server::world::terrain::decoration

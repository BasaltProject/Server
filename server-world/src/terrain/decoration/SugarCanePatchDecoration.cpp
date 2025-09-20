#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SugarCanePatchDecoration.hpp>

#include <algorithm>
#include <array>
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

const std::set<std::string, std::less<>>& sugarCaneSupport() {
    static const std::set<std::string, std::less<>> paths = {
        "dirt",       "coarse_dirt",     "rooted_dirt",    "mud",    "muddy_mangrove_roots",
        "moss_block", "pale_moss_block", "grass_block",    "podzol", "mycelium",
        "sand",       "red_sand",        "suspicious_sand"};
    return paths;
}

const std::vector<std::string>& defaultBiomes() {
    static const std::vector<std::string> biomes = {"bamboo_jungle",
                                                    "beach",
                                                    "birch_forest",
                                                    "cold_ocean",
                                                    "dark_forest",
                                                    "deep_cold_ocean",
                                                    "deep_frozen_ocean",
                                                    "deep_lukewarm_ocean",
                                                    "deep_ocean",
                                                    "flower_forest",
                                                    "forest",
                                                    "frozen_ocean",
                                                    "frozen_river",
                                                    "ice_spikes",
                                                    "jungle",
                                                    "lukewarm_ocean",
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
                                                    "windswept_savanna"};
    return biomes;
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::optional<std::int32_t> rarity;
    const std::vector<std::string>* biomes;
};

const std::vector<PatchSpec>& specs() {
    static const std::vector<std::string> desert = {"desert"};
    static const std::vector<std::string> badlands = {"badlands", "eroded_badlands", "wooded_badlands"};
    static const std::vector<std::string> swamp = {"swamp"};
    static const std::vector<PatchSpec> built = {{"patch_sugar_cane_desert", 75, std::nullopt, &desert},
                                                 {"patch_sugar_cane_badlands", 76, 5, &badlands},
                                                 {"patch_sugar_cane_swamp", 78, 3, &swamp},
                                                 {"patch_sugar_cane", 88, 6, &defaultBiomes()}};
    return built;
}

[[nodiscard]] bool isSugarCane(std::int32_t stateId) { return pathOf(stateId) == "sugar_cane"; }

[[nodiscard]] bool canPlaceSugarCane(const PatchRegion& region, const DecorationPos& position, ChunkHeight bounds) {
    if (position.y < bounds.minY() || position.y > bounds.maxY() || !registry::blocks::isAir(region.get(position))) {
        return false;
    }
    if (position.y == bounds.minY()) {
        return false;
    }
    const std::int32_t below = region.get(DecorationPos{position.x, position.y - 1, position.z});
    if (!isSugarCane(below) && sugarCaneSupport().find(pathOf(below)) == sugarCaneSupport().end()) {
        return false;
    }
    return std::ranges::any_of(kHorizontalOffsets, [&](const std::pair<std::int32_t, std::int32_t>& offset) {
        const std::int32_t adjacent =
            region.get(DecorationPos{position.x + offset.first, position.y - 1, position.z + offset.second});
        return registry::blockFluids::hasWaterFluid(adjacent) || pathOf(adjacent) == "frosted_ice";
    });
}

void placeColumn(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin, ChunkHeight bounds) {
    const std::int32_t height = 2 + random.nextInt(random.nextInt(3) + 1);
    std::int32_t actualHeight = height;
    for (std::int32_t dy = 1; dy <= height; ++dy) {
        const DecorationPos target{origin.x, origin.y + dy, origin.z};
        if (target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
            actualHeight = dy - 1;
            break;
        }
    }
    for (std::int32_t dy = 0; dy < actualHeight; ++dy) {
        region.set(DecorationPos{origin.x, origin.y + dy, origin.z}, stateIdOf("sugar_cane"));
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
        if (spec->rarity.has_value() && random.nextFloat() >= 1.0F / static_cast<float>(*spec->rarity)) {
            continue;
        }
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::MotionBlocking);
        if (std::ranges::find(*spec->biomes, biomePath(environment.biomeAt(x, y, z))) == spec->biomes->end()) {
            continue;
        }
        const DecorationPos origin{x, y, z};
        for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
            const DecorationPos target{origin.x + random.nextInt(5) - random.nextInt(5),
                                       origin.y + random.nextInt(1) - random.nextInt(1),
                                       origin.z + random.nextInt(5) - random.nextInt(5)};
            if (!canPlaceSugarCane(region, target, bounds)) {
                continue;
            }
            placeColumn(region, random, target, bounds);
        }
    }
}

} // namespace

void sugarCanePatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                        const surface::SurfaceRuleEnvironment& environment,
                                        ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void sugarCanePatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
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
        throw std::invalid_argument{"unknown sugar cane patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

} // namespace basalt::server::world::terrain::decoration

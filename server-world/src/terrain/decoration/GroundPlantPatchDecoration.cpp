#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GroundPlantPatchDecoration.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
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

const std::set<std::string, std::less<>>& supportsDryVegetation() {
    static const std::set<std::string, std::less<>> paths = {"dirt",
                                                             "coarse_dirt",
                                                             "rooted_dirt",
                                                             "mud",
                                                             "muddy_mangrove_roots",
                                                             "moss_block",
                                                             "pale_moss_block",
                                                             "grass_block",
                                                             "podzol",
                                                             "mycelium",
                                                             "farmland",
                                                             "sand",
                                                             "red_sand",
                                                             "suspicious_sand",
                                                             "terracotta",
                                                             "white_terracotta",
                                                             "orange_terracotta",
                                                             "magenta_terracotta",
                                                             "light_blue_terracotta",
                                                             "yellow_terracotta",
                                                             "lime_terracotta",
                                                             "pink_terracotta",
                                                             "gray_terracotta",
                                                             "light_gray_terracotta",
                                                             "cyan_terracotta",
                                                             "purple_terracotta",
                                                             "blue_terracotta",
                                                             "brown_terracotta",
                                                             "green_terracotta",
                                                             "red_terracotta",
                                                             "black_terracotta"};
    return paths;
}

enum class Provider : std::uint8_t { DeadBush, DryGrass };

[[nodiscard]] std::int32_t providerState(Provider provider, random::RandomSource& random) {
    if (provider == Provider::DeadBush) {
        return stateIdOf("dead_bush");
    }
    return random.nextInt(2) == 0 ? stateIdOf("short_dry_grass") : stateIdOf("tall_dry_grass");
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::int32_t outerCount;
    std::int32_t innerCount;
    std::vector<std::string> biomes;
    Provider provider;
    /// Zero means the spec has no rarity filter at all, not a filter that always passes.
    std::int32_t rarity = 0;
    HeightmapType heightmap = HeightmapType::WorldSurface;
};

const std::vector<PatchSpec>& specs() {
    static const std::vector<PatchSpec> built = {
        {"patch_dead_bush",
         65,
         1,
         4,
         {"mangrove_swamp", "old_growth_pine_taiga", "old_growth_spruce_taiga", "swamp"},
         Provider::DeadBush},
        {"patch_dry_grass_desert", 72, 1, 64, {"desert"}, Provider::DryGrass, 3, HeightmapType::MotionBlocking},
        {"patch_dead_bush_2", 73, 2, 4, {"desert"}, Provider::DeadBush},
        {"patch_dry_grass_badlands",
         74,
         1,
         64,
         {"badlands", "eroded_badlands", "wooded_badlands"},
         Provider::DryGrass,
         6,
         HeightmapType::MotionBlocking},
        {"patch_dead_bush_badlands",
         75,
         20,
         4,
         {"badlands", "eroded_badlands", "wooded_badlands"},
         Provider::DeadBush}};
    return built;
}

void placeCandidate(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                    const PatchSpec& spec, ChunkHeight bounds) {
    const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                               origin.y + random.nextInt(4) - random.nextInt(4),
                               origin.z + random.nextInt(8) - random.nextInt(8)};
    if (target.y < bounds.minY() || target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
        return;
    }
    // parity: the provider draws here, after the air check but before the support check, so a
    // candidate on unsuitable ground still consumes its value. Moving this below the support test
    // reads the same but desynchronises the stream.
    const std::int32_t stateId = providerState(spec.provider, random);
    if (target.y == bounds.minY()) {
        return;
    }
    const std::string belowPath = pathOf(region.get(DecorationPos{target.x, target.y - 1, target.z}));
    if (supportsDryVegetation().find(belowPath) == supportsDryVegetation().end()) {
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
        // parity: the three dead bush patches name `WORLD_SURFACE_WG`, the two dry grass ones
        // `MOTION_BLOCKING`.
        const std::int32_t y = spec.heightmap == HeightmapType::WorldSurface
                                   ? context.worldgenHeight(x & 15, z & 15, spec.heightmap)
                                   : context.height(x & 15, z & 15, spec.heightmap);
        const DecorationPos origin{x, y, z};
        if (std::ranges::find(spec.biomes, biomePath(environment.biomeAt(x, y, z))) == spec.biomes.end()) {
            continue;
        }
        for (std::int32_t inner = 0; inner < spec.innerCount; ++inner) {
            placeCandidate(region, random, origin, spec, bounds);
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

} // namespace

void groundPlantPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                          const surface::SurfaceRuleEnvironment& environment,
                                          ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void groundPlantPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
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
        throw std::invalid_argument{"unknown ground plant patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

const std::set<std::string, std::less<>>& groundPlantPatchDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = [] {
        std::set<std::string, std::less<>> built;
        for (const PatchSpec& spec : specs()) {
            built.emplace(spec.name);
        }
        return built;
    }();
    return ids;
}

bool groundPlantPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
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
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t inner = 0; inner < found->innerCount; ++inner) {
        placeCandidate(region, random, origin, *found, bounds);
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

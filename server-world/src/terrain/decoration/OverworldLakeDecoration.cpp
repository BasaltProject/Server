#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/decoration/OverworldBiomePaths.hpp>
#include <basalt/server/world/terrain/decoration/OverworldLakeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/StandaloneFeatures.hpp>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kLakesStep = 1;

constexpr std::int32_t kBelowTop = 319;

using PathSet = std::set<std::string, std::less<>>;

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

const PathSet& lavaLakeBiomes() {
    static const PathSet paths = [] {
        PathSet built = overworldBiomePaths();
        built.erase("deep_dark");
        return built;
    }();
    return paths;
}

const PathSet& sulfurCaves() {
    static const PathSet paths = {"sulfur_caves"};
    return paths;
}

[[nodiscard]] std::int32_t stateAt(const Chunk& chunk, const DecorationPos& position) {
    return chunk.decorationBlockStateId(position.x, position.y, position.z);
}

[[nodiscard]] std::int32_t sulfurStateId() {
    static const std::int32_t stateId = [] {
        const registry::BlockState* found = registry::blocks::named("sulfur");
        if (found == nullptr) {
            throw std::runtime_error{"unknown block 'sulfur'"};
        }
        return found->stateId;
    }();
    return stateId;
}

struct LakeSpec {
    std::string_view name;
    std::int32_t index;
    std::vector<PlacementModifier> modifiers;
    const PathSet* biomes;
};

const std::vector<LakeSpec>& specs() {
    static const std::vector<LakeSpec> built = [] {
        std::vector<LakeSpec> list;
        list.push_back(
            LakeSpec{.name = "lake_lava_underground",
                     .index = 0,
                     .modifiers = {rarityFilter(9), inSquarePlacement(), uniformHeightRangePlacement(0, kBelowTop),
                                   environmentScanPlacement(-1, 32,
                                                            [](std::int32_t stateId, const DecorationPos& position) {
                                                                return !registry::blocks::isAir(stateId) &&
                                                                       position.y - 5 >= kOverworldHeight.minY();
                                                            }),
                                   surfaceRelativeThresholdFilter(HeightmapType::OceanFloor,
                                                                  std::numeric_limits<std::int32_t>::min(), -5)},
                     .biomes = &lavaLakeBiomes()});
        list.push_back(LakeSpec{.name = "lake_lava_surface",
                                .index = 1,
                                .modifiers = {rarityFilter(200), inSquarePlacement(),
                                              worldgenHeightmapPlacement(HeightmapType::WorldSurface)},
                                .biomes = &lavaLakeBiomes()});
        list.push_back(LakeSpec{.name = "rooted_sulfur_spring",
                                .index = 2,
                                .modifiers = {countPlacement(IntProvider::uniform(1, 2)), inSquarePlacement(),
                                              uniformHeightRangePlacement(kOverworldHeight.minY(), 256),
                                              environmentScanPlacement(
                                                  1, 12,
                                                  [](std::int32_t stateId, const DecorationPos&) {
                                                      return registry::blockShapes::isSolidState(stateId);
                                                  },
                                                  [](std::int32_t stateId, const DecorationPos&) {
                                                      return registry::blocks::isAir(stateId);
                                                  }),
                                              constantOffsetPlacement(0, -1)},
                                .biomes = &sulfurCaves()});
        list.push_back(LakeSpec{
            .name = "sulfur_pool",
            .index = 3,
            .modifiers =
                {countPlacement(256), inSquarePlacement(), uniformHeightRangePlacement(kOverworldHeight.minY(), 256),
                 filter([](const PlacementContext& context, random::RandomSource&, const DecorationPos& origin) {
                     return registry::blockShapes::isSolidState(stateAt(context.chunk(), origin));
                 }),
                 environmentScanPlacement(
                     1, 32,
                     [](std::int32_t stateId, const DecorationPos&) { return registry::blocks::isAir(stateId); }),
                 constantOffsetPlacement(0, -1),
                 filter([](const PlacementContext& context, random::RandomSource&, const DecorationPos& origin) {
                     return stateAt(context.chunk(), origin) == sulfurStateId();
                 })},
            .biomes = &sulfurCaves()});
        return list;
    }();
    return built;
}

void decorateSpec(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                  const LakeSpec& spec) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, spec.index, kLakesStep);
    const DecorationPos origin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
    placeFrom(chunk, random, origin, spec.modifiers, [&](const DecorationPos& position) {
        const std::string_view biome = biomePath(environment.biomeAt(position.x, position.y, position.z));
        if (spec.biomes->find(biome) == spec.biomes->end()) {
            return;
        }
        // parity: `rooted_sulfur_spring` needs templates, so only its placement draw runs here.
        static_cast<void>(standaloneFeatures::placeAt(chunk, random, position, spec.name));
    });
}

} // namespace

void overworldLakeDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                       const surface::SurfaceRuleEnvironment& environment) {
    for (const LakeSpec& spec : specs()) {
        decorateSpec(chunk, seed, environment, spec);
    }
}

void overworldLakeDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                              const surface::SurfaceRuleEnvironment& environment,
                                              std::string_view name) {
    for (const LakeSpec& spec : specs()) {
        if (spec.name == name) {
            decorateSpec(chunk, seed, environment, spec);
            return;
        }
    }
    throw std::runtime_error{"unknown lake feature '" + std::string{name} + "'"};
}

} // namespace basalt::server::world::terrain::decoration

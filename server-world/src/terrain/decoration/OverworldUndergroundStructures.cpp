#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldBiomePaths.hpp>
#include <basalt/server/world/terrain/decoration/OverworldUndergroundStructures.hpp>
#include <basalt/server/world/terrain/decoration/StandaloneFeatures.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

using structure::BlockRot;
using structure::StructureProcessorList;
using structure::StructureProcessorRule;
using structure::StructureRuleTest;

constexpr std::int32_t kRarity = 64;
constexpr std::int32_t kMaxEmptyCorners = 4;
constexpr std::int32_t kUndergroundStructuresStep = 3;
/// `below_top 0`, the highest block the world can generate.
constexpr std::int32_t kBelowTop = 319;

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

/// parity: `#minecraft:features_cannot_replace`, the tag all three fossil lists protect.
[[nodiscard]] std::set<std::int32_t> protectedStates() {
    std::set<std::int32_t> states;
    for (const std::string_view path :
         {"bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault"}) {
        if (const registry::BlockState* found = registry::blocks::named(path); found != nullptr) {
            states.insert(found->stateId);
        }
    }
    return states;
}

const std::vector<std::string>& fossilBase() {
    static const std::vector<std::string> names = {"fossil/spine_1", "fossil/spine_2", "fossil/spine_3",
                                                   "fossil/spine_4", "fossil/skull_1", "fossil/skull_2",
                                                   "fossil/skull_3", "fossil/skull_4"};
    return names;
}

const std::vector<std::string>& fossilOverlay() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> built;
        built.reserve(fossilBase().size());
        for (const std::string& name : fossilBase()) {
            built.push_back(name + "_coal");
        }
        return built;
    }();
    return names;
}

struct FossilSpec {
    std::int32_t index;
    std::int32_t minY;
    std::int32_t maxY;
    fossilFeature::Spec spec;
};

const std::vector<FossilSpec>& fossils(std::int32_t worldMinY) {
    static const std::vector<FossilSpec> built = [worldMinY] {
        /// parity: `processor_list/fossil_rot`, the bones, which keep nine blocks in ten.
        const StructureProcessorList rot{{}, BlockRot{.integrity = 0.9F}, protectedStates()};
        /// parity: `processor_list/fossil_coal`, one block in ten of the ore overlay.
        const StructureProcessorList coal{{}, BlockRot{.integrity = 0.1F}, protectedStates()};
        /// parity: `processor_list/fossil_diamonds`, the same tenth with its coal swapped out.
        const StructureProcessorList diamonds{
            {StructureProcessorRule{.inputTest = StructureRuleTest::blockMatch(state("coal_ore")),
                                    .locationTest = std::nullopt,
                                    .outputStateId = state("deepslate_diamond_ore")}},
            BlockRot{.integrity = 0.1F},
            protectedStates()};
        std::vector<FossilSpec> specs;
        // `fossil_upper`: absolute 0 up to `below_top 0`, with the coal overlay.
        specs.push_back(FossilSpec{.index = 0,
                                   .minY = 0,
                                   .maxY = kBelowTop,
                                   .spec = fossilFeature::Spec{.structures = fossilBase(),
                                                               .overlays = fossilOverlay(),
                                                               .fossilProcessors = rot,
                                                               .overlayProcessors = coal,
                                                               .maxEmptyCornersAllowed = kMaxEmptyCorners}});
        // `fossil_lower`: `above_bottom 0` up to y -8, where the coal becomes diamond.
        specs.push_back(FossilSpec{.index = 1,
                                   .minY = worldMinY,
                                   .maxY = -8,
                                   .spec = fossilFeature::Spec{.structures = fossilBase(),
                                                               .overlays = fossilOverlay(),
                                                               .fossilProcessors = rot,
                                                               .overlayProcessors = diamonds,
                                                               .maxEmptyCornersAllowed = kMaxEmptyCorners}});
        return specs;
    }();
    return built;
}

/// parity: the only three biomes whose generation settings list the two fossils. The monster rooms
/// share the step but not the biomes, which is why this is not the rooms' own set.
const std::set<std::string, std::less<>>& fossilBiomes() {
    static const std::set<std::string, std::less<>> biomes = {"desert", "swamp", "mangrove_swamp"};
    return biomes;
}

struct RoomSpec {
    std::int32_t index;
    std::int32_t count;
    std::int32_t minY;
    std::int32_t maxY;
};

/// parity: `fossil_upper` and `fossil_lower`, whose chain is a rarity filter and not a count: one
/// attempt in 64 chunks, and the draws below only happen in that chunk.
void decorateFossil(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                    const FossilSpec& fossil, const fossilFeature::TemplateLookup& templates,
                    const fossilFeature::OceanFloorAt& externalOceanFloorHeight) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, fossil.index, kUndergroundStructuresStep);
    if (random.nextFloat() >= 1.0F / static_cast<float>(kRarity)) {
        return;
    }
    const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
    const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
    const std::int32_t y = fossil.minY + random.nextInt(fossil.maxY - fossil.minY + 1);
    if (!fossilBiomes().contains(biomePath(environment.biomeAt(x, y, z)))) {
        return;
    }
    static_cast<void>(fossilFeature::place(
        chunk, random, structure::StructurePos{x, y, z}, fossil.spec, templates,
        [&chunk, &externalOceanFloorHeight](std::int32_t columnX, std::int32_t columnZ) -> std::optional<std::int32_t> {
            if ((columnX >> 4) == chunk.chunkX() && (columnZ >> 4) == chunk.chunkZ()) {
                // parity: the fossil reads `OCEAN_FLOOR_WG`, the frozen map, not the live one: an
                // ore blob or another fossil placed first must not raise the floor it buries to.
                return chunk.worldgenHeightAt(columnX & 15, columnZ & 15, HeightmapType::OceanFloor);
            }
            return externalOceanFloorHeight ? externalOceanFloorHeight(columnX, columnZ) : std::nullopt;
        }));
}

void decorateRoom(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                  const RoomSpec& spec) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, spec.index, kUndergroundStructuresStep);
    for (std::int32_t attempt = 0; attempt < spec.count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = spec.minY + random.nextInt(spec.maxY - spec.minY + 1);
        if (!overworldBiomePaths().contains(biomePath(environment.biomeAt(x, y, z)))) {
            continue;
        }
        static_cast<void>(standaloneFeatures::placeAt(chunk, random, DecorationPos{x, y, z}, "monster_room"));
    }
}

} // namespace

void overworldUndergroundStructures::decorate(Chunk& chunk, const std::int64_t seed,
                                              const surface::SurfaceRuleEnvironment& environment,
                                              const fossilFeature::TemplateLookup& templates,
                                              const fossilFeature::OceanFloorAt& externalOceanFloorHeight) {
    if (templates) {
        for (const FossilSpec& fossil : fossils(chunk.verticalBounds().minY())) {
            decorateFossil(chunk, seed, environment, fossil, templates, externalOceanFloorHeight);
        }
    }
    const std::vector<RoomSpec> rooms{
        RoomSpec{.index = 2, .count = 10, .minY = 0, .maxY = kBelowTop},
        // `above_bottom 6` up to y -1, the deep pass.
        RoomSpec{.index = 3, .count = 4, .minY = chunk.verticalBounds().minY() + 6, .maxY = -1}};
    for (const RoomSpec& spec : rooms) {
        decorateRoom(chunk, seed, environment, spec);
    }
}

} // namespace basalt::server::world::terrain::decoration

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/MelonPatchDecoration.hpp>

#include <algorithm>
#include <cstdint>
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

/// parity: the feature's own replaceable list, which is wider than "is air": a melon takes over
/// grass, vines and the other soft blocks a jungle floor is covered in.
const std::set<std::string, std::less<>>& replaceableBlocks() {
    static const std::set<std::string, std::less<>> paths = {"air",
                                                             "water",
                                                             "lava",
                                                             "short_grass",
                                                             "fern",
                                                             "dead_bush",
                                                             "bush",
                                                             "short_dry_grass",
                                                             "tall_dry_grass",
                                                             "seagrass",
                                                             "tall_seagrass",
                                                             "fire",
                                                             "soul_fire",
                                                             "snow",
                                                             "vine",
                                                             "glow_lichen",
                                                             "resin_clump",
                                                             "light",
                                                             "tall_grass",
                                                             "large_fern",
                                                             "structure_void",
                                                             "void_air",
                                                             "cave_air",
                                                             "bubble_column",
                                                             "warped_roots",
                                                             "nether_sprouts",
                                                             "crimson_roots",
                                                             "leaf_litter",
                                                             "hanging_roots"};
    return paths;
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::int32_t rarity;
    std::vector<std::string> biomes;
};

const std::vector<PatchSpec>& specs() {
    static const std::vector<PatchSpec> built = {{"patch_melon_sparse", 92, 64, {"sparse_jungle"}},
                                                 {"patch_melon", 93, 6, {"bamboo_jungle", "jungle"}}};
    return built;
}

void placeMelonPatch(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                     ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(8) - random.nextInt(8)};
        if (target.y < bounds.minY() || target.y > bounds.maxY()) {
            continue;
        }
        const std::int32_t current = region.get(target);
        if (replaceableBlocks().find(pathOf(current)) == replaceableBlocks().end() ||
            registry::blockFluids::hasFluid(current)) {
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
        region.set(target, stateIdOf("melon"));
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
        if (random.nextFloat() >= 1.0F / static_cast<float>(spec->rarity)) {
            continue;
        }
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::MotionBlocking);
        if (std::ranges::find(spec->biomes, biomePath(environment.biomeAt(x, y, z))) == spec->biomes.end()) {
            continue;
        }
        placeMelonPatch(region, random, DecorationPos{x, y, z}, bounds);
    }
}

} // namespace

void melonPatchDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                    ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void melonPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                           const surface::SurfaceRuleEnvironment& environment, std::string_view name,
                                           ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : specs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown melon patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void melonPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    PatchRegion region{chunk, nullptr, nullptr};
    placeMelonPatch(region, random, origin, chunk.verticalBounds());
}

} // namespace basalt::server::world::terrain::decoration

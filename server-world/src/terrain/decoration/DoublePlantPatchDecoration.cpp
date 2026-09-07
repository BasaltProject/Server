#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/DoublePlantPatchDecoration.hpp>

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

const std::set<std::string, std::less<>>& supportsVegetation() {
    static const std::set<std::string, std::less<>> paths = {
        "dirt",        "coarse_dirt", "rooted_dirt", "mud",     "muddy_mangrove_roots", "moss_block", "pale_moss_block",
        "grass_block", "podzol",      "mycelium",    "farmland"};
    return paths;
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::int32_t rarity;
    std::vector<std::string> biomes;
    std::int32_t lowerStateId;
    std::int32_t upperStateId;
};

[[nodiscard]] PatchSpec makeSpec(std::string_view name, std::int32_t index, std::int32_t rarity,
                                 std::vector<std::string> biomes, std::string_view path) {
    const std::int32_t lower = stateIdOf(path);
    const std::optional<std::int32_t> upper = registry::blockStateFamilies::upperDoublePlantStateId(lower);
    if (!upper.has_value()) {
        throw std::runtime_error{"'" + std::string{path} + "' has no upper half"};
    }
    return PatchSpec{name, index, rarity, std::move(biomes), lower, *upper};
}

const std::vector<PatchSpec>& specs() {
    static const std::vector<PatchSpec> built = {
        makeSpec("patch_sunflower", 34, 3, {"sunflower_plains"}, "sunflower"),
        makeSpec("patch_large_fern", 40, 5,
                 {"old_growth_pine_taiga", "old_growth_spruce_taiga", "snowy_taiga", "taiga"}, "large_fern")};
    return built;
}

void placeCandidate(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                    const PatchSpec& spec, ChunkHeight bounds) {
    const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                               origin.y + random.nextInt(4) - random.nextInt(4),
                               origin.z + random.nextInt(8) - random.nextInt(8)};
    // The upper half needs a block of its own, so the top of the world is excluded rather than
    // clamped: a plant that cannot fit is not placed at all.
    if (target.y < bounds.minY() || target.y >= bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
        return;
    }
    const DecorationPos upper{target.x, target.y + 1, target.z};
    if (!registry::blocks::isAir(region.get(upper))) {
        return;
    }
    const std::string belowPath = pathOf(region.get(DecorationPos{target.x, target.y - 1, target.z}));
    if (supportsVegetation().find(belowPath) == supportsVegetation().end()) {
        return;
    }
    region.set(target, spec.lowerStateId);
    region.set(upper, spec.upperStateId);
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
        const DecorationPos origin{x, y, z};
        for (std::int32_t attempt = 0; attempt < 96; ++attempt) {
            placeCandidate(region, random, origin, *spec, bounds);
        }
    }
}

} // namespace

void doublePlantPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                          const surface::SurfaceRuleEnvironment& environment,
                                          ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void doublePlantPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
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
        throw std::invalid_argument{"unknown double plant patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

const std::set<std::string, std::less<>>& doublePlantPatchDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = [] {
        std::set<std::string, std::less<>> built;
        for (const PatchSpec& spec : specs()) {
            built.emplace(spec.name);
        }
        return built;
    }();
    return ids;
}

bool doublePlantPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
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
    for (std::int32_t attempt = 0; attempt < 96; ++attempt) {
        placeCandidate(region, random, origin, *found, bounds);
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

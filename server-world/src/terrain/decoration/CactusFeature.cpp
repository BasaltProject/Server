#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CactusFeature.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
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

const std::set<std::string, std::less<>>& cactusSupport() {
    static const std::set<std::string, std::less<>> paths = {"cactus", "sand", "red_sand", "suspicious_sand"};
    return paths;
}

[[nodiscard]] bool isLava(std::int32_t stateId) {
    const std::int32_t lava = registry::blocks::lava().stateId;
    return stateId >= lava && stateId <= lava + 15;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

struct PatchSpec {
    std::string_view name;
    std::int32_t index;
    std::int32_t rarity;
    std::vector<std::string> biomes;
};

const std::vector<PatchSpec>& specs() {
    static const std::vector<PatchSpec> built = {
        {"patch_cactus_desert", 86, 6, {"desert"}},
        {"patch_cactus_decorated", 87, 13, {"badlands", "eroded_badlands", "wooded_badlands"}}};
    return built;
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
        for (std::int32_t candidate = 0; candidate < 10; ++candidate) {
            const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                       origin.y + random.nextInt(4) - random.nextInt(4),
                                       origin.z + random.nextInt(8) - random.nextInt(8)};
            if (!cactusFeature::canPlace(region, target, bounds)) {
                continue;
            }
            cactusFeature::placeColumn(region, random, target, bounds);
        }
    }
}

} // namespace

bool cactusFeature::canPlace(const PatchRegion& region, const DecorationPos& origin, ChunkHeight bounds) {
    if (origin.y < bounds.minY() || origin.y > bounds.maxY() || !registry::blocks::isAir(region.get(origin))) {
        return false;
    }
    for (const auto& [dx, dz] : kHorizontalOffsets) {
        const std::int32_t neighbour = region.get(DecorationPos{origin.x + dx, origin.y, origin.z + dz});
        if (registry::blockShapes::isSolidState(neighbour) || isLava(neighbour)) {
            return false;
        }
    }
    if (origin.y == bounds.minY()) {
        return false;
    }
    const std::string belowPath = pathOf(region.get(DecorationPos{origin.x, origin.y - 1, origin.z}));
    if (cactusSupport().find(belowPath) == cactusSupport().end()) {
        return false;
    }
    return !registry::blockFluids::hasFluid(region.get(DecorationPos{origin.x, origin.y + 1, origin.z}));
}

bool cactusFeature::canPlace(Chunk& chunk, const DecorationPos& origin) {
    const PatchRegion region{chunk, nullptr, nullptr};
    return canPlace(region, origin, chunk.verticalBounds());
}

bool cactusFeature::placeColumn(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                                ChunkHeight bounds) {
    // parity: BiasedToBottomInt.sample is min + nextInt(nextInt(max - min + 1) + 1); the flower
    // layer's weighted list draws nextInt(4) with only the last slot blooming.
    const std::int32_t cactusHeight = 1 + random.nextInt(random.nextInt(3) + 1);
    const std::int32_t flowerHeight = random.nextInt(4) == 3 ? 1 : 0;
    const std::int32_t totalHeight = cactusHeight + flowerHeight;

    // parity: the allowed-placement walk checks air above the origin and truncates the flower layer
    // first, then the cactus body.
    std::int32_t newHeight = totalHeight;
    for (std::int32_t dy = 1; dy <= totalHeight; ++dy) {
        const DecorationPos position{origin.x, origin.y + dy, origin.z};
        if (position.y > bounds.maxY() || !registry::blocks::isAir(region.get(position))) {
            newHeight = dy - 1;
            break;
        }
    }
    std::int32_t flowers = flowerHeight;
    std::int32_t cacti = cactusHeight;
    std::int32_t toRemove = totalHeight - newHeight;
    const std::int32_t trimmedFlowers = std::min(flowers, toRemove);
    flowers -= trimmedFlowers;
    toRemove -= trimmedFlowers;
    cacti -= std::min(cacti, toRemove);

    std::int32_t y = origin.y;
    for (std::int32_t placed = 0; placed < cacti; ++placed) {
        region.set(DecorationPos{origin.x, y, origin.z}, stateIdOf("cactus"));
        ++y;
    }
    for (std::int32_t placed = 0; placed < flowers; ++placed) {
        region.set(DecorationPos{origin.x, y, origin.z}, stateIdOf("cactus_flower"));
        ++y;
    }
    return true;
}

bool cactusFeature::place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (origin.y < bounds.minY() || origin.y > bounds.maxY()) {
        return false;
    }
    PatchRegion region{chunk, nullptr, nullptr};
    if (!canPlace(region, origin, bounds)) {
        return false;
    }
    return placeColumn(region, random, origin, bounds);
}

void cactusPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                     const surface::SurfaceRuleEnvironment& environment,
                                     ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    chosen.reserve(specs().size());
    for (const PatchSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

void cactusPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                            const surface::SurfaceRuleEnvironment& environment, std::string_view name,
                                            ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    std::vector<const PatchSpec*> chosen;
    for (const PatchSpec& spec : specs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown cactus patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen, std::move(externalBlockState), std::move(observer));
}

} // namespace basalt::server::world::terrain::decoration

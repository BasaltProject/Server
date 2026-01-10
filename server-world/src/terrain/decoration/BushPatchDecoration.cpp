#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/BushPatchDecoration.hpp>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kBushIndex = 51;

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

const std::set<std::string, std::less<>>& biomes() {
    static const std::set<std::string, std::less<>> paths = {
        "birch_forest",   "forest", "frozen_river",     "old_growth_birch_forest",
        "plains",         "river",  "windswept_forest", "windswept_gravelly_hills",
        "windswept_hills"};
    return paths;
}

void placeBushPatch(PatchRegion& region, random::RandomSource& random, const DecorationPos& origin,
                    ChunkHeight bounds) {
    for (std::int32_t attempt = 0; attempt < 24; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(6) - random.nextInt(6),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(6) - random.nextInt(6)};
        if (target.y < bounds.minY() || target.y > bounds.maxY() || !registry::blocks::isAir(region.get(target))) {
            continue;
        }
        if (target.y == bounds.minY()) {
            continue;
        }
        const std::string belowPath = pathOf(region.get(DecorationPos{target.x, target.y - 1, target.z}));
        if (supportsVegetation().find(belowPath) == supportsVegetation().end()) {
            continue;
        }
        region.set(target, stateIdOf("bush"));
    }
}

} // namespace

void bushPatchDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                   ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kBushIndex, kVegetalDecorationStep);
    if (random.nextFloat() >= 1.0F / 4.0F) {
        return;
    }
    const PlacementContext context{chunk};
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};
    const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
    const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
    const std::int32_t y = context.height(x & 15, z & 15, HeightmapType::MotionBlocking);
    if (biomes().find(biomePath(environment.biomeAt(x, y, z))) == biomes().end()) {
        return;
    }
    placeBushPatch(region, random, DecorationPos{x, y, z}, chunk.verticalBounds());
}

void bushPatchDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    PatchRegion region{chunk, nullptr, nullptr};
    placeBushPatch(region, random, origin, chunk.verticalBounds());
}

} // namespace basalt::server::world::terrain::decoration

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/VinePatchDecoration.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::int32_t kFeatureIndex = 94;

struct Direction {
    registry::BlockFace face;
    /// The bit that is cleared from the default state to turn that face on: vanilla's vine states
    /// count down from all-faces-set, so a face is enabled by subtracting its false bit.
    std::int32_t falseBit;
};

constexpr std::array<Direction, 5> kDirections = {{{registry::BlockFace::Up, 2},
                                                   {registry::BlockFace::North, 8},
                                                   {registry::BlockFace::South, 4},
                                                   {registry::BlockFace::West, 1},
                                                   {registry::BlockFace::East, 16}}};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

const std::set<std::string, std::less<>>& biomes() {
    static const std::set<std::string, std::less<>> paths = {"bamboo_jungle", "jungle", "sparse_jungle"};
    return paths;
}

void placeVineAt(PatchRegion& region, const DecorationPos& position) {
    if (!registry::blocks::isAir(region.get(position))) {
        return;
    }
    for (const Direction& direction : kDirections) {
        const DecorationPos neighbourPosition{position.x + registry::stepX(direction.face),
                                              position.y + registry::stepY(direction.face),
                                              position.z + registry::stepZ(direction.face)};
        const std::int32_t neighbour = region.get(neighbourPosition);
        const registry::BlockFace attachmentFace = registry::opposite(direction.face);
        const bool acceptable =
            registry::blockShapes::isFaceSturdy(neighbour, attachmentFace) ||
            registry::blockShapes::faceCovers(neighbour, attachmentFace, 0, registry::blockShapes::kUnits, 0,
                                              registry::blockShapes::kUnits);
        if (!acceptable) {
            continue;
        }
        region.set(position, stateIdOf("vine") - direction.falseBit);
        return;
    }
}

} // namespace

void vinePatchDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                   ExternalStateReader externalBlockState, BlockWriteObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kFeatureIndex, kVegetalDecorationStep);
    PatchRegion region{chunk, std::move(externalBlockState), std::move(observer)};

    for (std::int32_t attempt = 0; attempt < 127; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const DecorationPos position{x, 64 + random.nextInt(37), z};
        if (biomes().find(biomePath(environment.biomeAt(position.x, position.y, position.z))) == biomes().end()) {
            continue;
        }
        placeVineAt(region, position);
    }
}

void vinePatchDecoration::placeCaveAt(Chunk& chunk, const DecorationPos& position) {
    PatchRegion region{chunk, nullptr, nullptr};
    placeVineAt(region, position);
}

} // namespace basalt::server::world::terrain::decoration

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/PaleMossDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVerticalRange = 5;
constexpr float kVegetationChance = 0.3F;
constexpr float kExtraEdgeColumnChance = 0.75F;
constexpr std::int32_t kPaleMossPatchIndex = 14;
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

const std::set<std::string, std::less<>>& mossReplaceablePaths() {
    static const std::set<std::string, std::less<>> paths = {"stone",
                                                             "granite",
                                                             "diorite",
                                                             "andesite",
                                                             "tuff",
                                                             "deepslate",
                                                             "cave_vines_plant",
                                                             "cave_vines",
                                                             "dirt",
                                                             "coarse_dirt",
                                                             "rooted_dirt",
                                                             "mud",
                                                             "muddy_mangrove_roots",
                                                             "moss_block",
                                                             "pale_moss_block",
                                                             "grass_block",
                                                             "podzol",
                                                             "mycelium"};
    return paths;
}

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    return y >= bounds.minY() && y <= bounds.maxY() && registry::blocks::isAir(chunk.decorationBlockStateId(x, y, z));
}

/// parity: `VegetationPatchFeature.getSurfacePos`, which walks up out of the air and then back down
/// onto the first sturdy face.
[[nodiscard]] std::optional<DecorationPos> findGround(const Chunk& chunk, std::int32_t x, std::int32_t startY,
                                                      std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t y = startY;
    for (std::int32_t offset = 0; isAir(chunk, x, y, z) && offset < kVerticalRange; ++offset) {
        y--;
        if (y < bounds.minY()) {
            return std::nullopt;
        }
    }
    for (std::int32_t offset = 0; !isAir(chunk, x, y, z) && offset < kVerticalRange; ++offset) {
        y++;
        if (y > bounds.maxY()) {
            return std::nullopt;
        }
    }
    const std::int32_t belowY = y - 1;
    if (!isAir(chunk, x, y, z) || belowY < bounds.minY()) {
        return std::nullopt;
    }
    if (!registry::blockShapes::isFaceSturdy(chunk.decorationBlockStateId(x, belowY, z), registry::BlockFace::Up)) {
        return std::nullopt;
    }
    return DecorationPos{.x = x, .y = belowY, .z = z};
}

/// The bucket iteration order of vanilla's `HashSet<BlockPos>`, which is observable here because the
/// vegetation draws once per position.
[[nodiscard]] std::vector<DecorationPos> javaHashSetOrder(const std::vector<DecorationPos>& positions) {
    if (positions.empty()) {
        return {};
    }
    std::size_t capacity = 16;
    while (positions.size() > capacity * 3 / 4) {
        capacity *= 2;
    }
    std::vector<DecorationPos> ordered;
    ordered.reserve(positions.size());
    for (std::size_t bucket = 0; bucket < capacity; ++bucket) {
        for (const DecorationPos& position : positions) {
            const std::int32_t hash = (position.y + position.z * 31) * 31 + position.x;
            const std::uint32_t spread = static_cast<std::uint32_t>(hash) ^ (static_cast<std::uint32_t>(hash) >> 16U);
            if ((spread & static_cast<std::uint32_t>(capacity - 1)) == bucket) {
                ordered.push_back(position);
            }
        }
    }
    return ordered;
}

void placeVegetation(Chunk& chunk, random::RandomSource& random, random::RandomSource& mossyCarpetRandom,
                     const DecorationPos& ground) {
    const std::int32_t roll = random.nextInt(60);
    std::int32_t stateId = stateIdOf("tall_grass");
    if (roll < 25) {
        stateId = stateIdOf("pale_moss_carpet");
    } else if (roll < 50) {
        stateId = stateIdOf("short_grass");
    }
    const SimpleBlockFeature feature{[stateId](random::RandomSource&, const DecorationPos&) { return stateId; },
                                     stateId == stateIdOf("tall_grass")};
    feature.place(chunk, random, DecorationPos{.x = ground.x, .y = ground.y + 1, .z = ground.z}, mossyCarpetRandom);
}

/// parity: `WorldGenRegion.random`, positioned at the centre chunk's minimum block corner.
[[nodiscard]] random::RandomSource worldgenRegionRandom(std::int64_t seed, const Chunk& chunk) {
    random::RandomSource source = random::RandomSource::xoroshiro(seed);
    random::RandomSource named = source.forkPositional().fromHashOf("minecraft:worldgen_region_random");
    return named.forkPositional().at(chunk.chunkX() * 16, 0, chunk.chunkZ() * 16);
}

const std::vector<PlacementModifier>& scatterModifiers() {
    static const std::vector<PlacementModifier> modifiers{countPlacement(1), inSquarePlacement(),
                                                          heightmapPlacement(HeightmapType::MotionBlockingNoLeaves)};
    return modifiers;
}

} // namespace

void paleMossDecoration::placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                      random::RandomSource& mossyCarpetRandom) {
    // parity: VegetationPatchFeature adds one to each configured UniformInt(2..4) sample.
    const std::int32_t xRadius = 3 + random.nextInt(3);
    const std::int32_t zRadius = 3 + random.nextInt(3);
    std::vector<DecorationPos> surface;
    surface.reserve(static_cast<std::size_t>((xRadius * 2 + 1) * (zRadius * 2 + 1)));

    for (std::int32_t dx = -xRadius; dx <= xRadius; ++dx) {
        const bool xEdge = dx == -xRadius || dx == xRadius;
        for (std::int32_t dz = -zRadius; dz <= zRadius; ++dz) {
            const bool zEdge = dz == -zRadius || dz == zRadius;
            const bool corner = xEdge && zEdge;
            const bool edgeOnly = (xEdge || zEdge) && !corner;
            if (corner || (edgeOnly && random.nextFloat() > kExtraEdgeColumnChance)) {
                continue;
            }
            const std::optional<DecorationPos> ground = findGround(chunk, origin.x + dx, origin.y, origin.z + dz);
            if (!ground.has_value()) {
                continue;
            }
            const std::int32_t previous = chunk.decorationBlockStateId(ground->x, ground->y, ground->z);
            if (!mossReplaceablePaths().contains(pathOf(previous))) {
                continue;
            }
            if (previous != stateIdOf("pale_moss_block")) {
                chunk.setDecorationBlockStateId(ground->x, ground->y, ground->z, stateIdOf("pale_moss_block"));
            }
            surface.push_back(*ground);
        }
    }

    for (const DecorationPos& ground : javaHashSetOrder(surface)) {
        if (random.nextFloat() < kVegetationChance) {
            placeVegetation(chunk, random, mossyCarpetRandom, ground);
        }
    }
}

void paleMossDecoration::placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    placePatchAt(chunk, random, origin, random);
}

void paleMossDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    placeVegetation(chunk, random, random, origin);
}

void paleMossDecoration::decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                                  PlacementObserver observer) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    random::RandomSource mossyCarpetRandom = worldgenRegionRandom(seed, chunk);
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kPaleMossPatchIndex, kVegetalDecorationStep);
    const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    placeFrom(chunk, random, chunkOrigin, scatterModifiers(), [&](const DecorationPos& origin) {
        if (biomePath(environment.biomeAt(origin.x, origin.y, origin.z)) != "pale_garden") {
            return;
        }
        if (observer != nullptr) {
            observer(origin);
        }
        placePatchAt(chunk, random, origin, mossyCarpetRandom);
    });
}

void paleMossDecoration::decorate(Chunk& chunk, random::RandomSource& random) {
    const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    placeFrom(chunk, random, chunkOrigin, scatterModifiers(),
              [&](const DecorationPos& origin) { placePatchAt(chunk, random, origin, random); });
}

} // namespace basalt::server::world::terrain::decoration

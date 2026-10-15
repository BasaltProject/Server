#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveVineColumnFeature.hpp>
#include <basalt/server/world/terrain/decoration/JavaHashSetOrder.hpp>
#include <basalt/server/world/terrain/decoration/LushVegetationPatchFeature.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

using PathSet = std::set<std::string, std::less<>>;

enum class Vegetation : std::uint8_t { CaveVine, Moss, Dripleaf };

struct PatchConfig {
    std::int32_t groundStateId = 0;
    const PathSet* replaceable = nullptr;
    std::int32_t minDepth = 0;
    std::int32_t maxDepth = 0;
    float extraBottomBlockChance = 0.0F;
    std::int32_t verticalRange = 0;
    float vegetationChance = 0.0F;
    float extraEdgeColumnChance = 0.0F;
    std::int32_t inwards = 0;
    Vegetation vegetation = Vegetation::Moss;
    bool waterlogged = false;
};

constexpr std::array<registry::BlockFace, 5> kExposureDirections{registry::BlockFace::North, registry::BlockFace::East,
                                                                 registry::BlockFace::South, registry::BlockFace::West,
                                                                 registry::BlockFace::Down};

constexpr std::array<std::string_view, 4> kFacingNames{"east", "west", "north", "south"};
constexpr std::array<std::string_view, 4> kBigDripleafFacingNames{"east", "west", "south", "north"};

[[nodiscard]] std::int32_t state(std::string_view path) {
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

const PathSet& mossReplaceable() {
    static const PathSet paths = {"stone",
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

const PathSet& lushGroundReplaceable() {
    static const PathSet paths = [] {
        PathSet built = mossReplaceable();
        built.insert("clay");
        built.insert("gravel");
        built.insert("sand");
        return built;
    }();
    return paths;
}

const PathSet& supportsSmallDripleafPaths() {
    static const PathSet paths = {"clay", "moss_block"};
    return paths;
}

const PathSet& supportsVegetationPaths() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt", "mud",    "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "grass_block", "podzol", "mycelium",
                                  "farmland"};
    return paths;
}

const PatchConfig& ceilingMoss() {
    static const PatchConfig config{.groundStateId = state("moss_block"),
                                    .replaceable = &mossReplaceable(),
                                    .minDepth = 1,
                                    .maxDepth = 2,
                                    .extraBottomBlockChance = 0.0F,
                                    .verticalRange = 5,
                                    .vegetationChance = 0.08F,
                                    .extraEdgeColumnChance = 0.3F,
                                    .inwards = 1,
                                    .vegetation = Vegetation::CaveVine};
    return config;
}

const PatchConfig& floorMoss() {
    static const PatchConfig config{.groundStateId = state("moss_block"),
                                    .replaceable = &mossReplaceable(),
                                    .minDepth = 1,
                                    .maxDepth = 1,
                                    .extraBottomBlockChance = 0.0F,
                                    .verticalRange = 5,
                                    .vegetationChance = 0.8F,
                                    .extraEdgeColumnChance = 0.3F,
                                    .inwards = -1,
                                    .vegetation = Vegetation::Moss};
    return config;
}

const PatchConfig& clay() {
    static const PatchConfig config{.groundStateId = state("clay"),
                                    .replaceable = &lushGroundReplaceable(),
                                    .minDepth = 3,
                                    .maxDepth = 3,
                                    .extraBottomBlockChance = 0.8F,
                                    .verticalRange = 2,
                                    .vegetationChance = 0.05F,
                                    .extraEdgeColumnChance = 0.7F,
                                    .inwards = -1,
                                    .vegetation = Vegetation::Dripleaf};
    return config;
}

const PatchConfig& waterloggedClay() {
    static const PatchConfig config{.groundStateId = state("clay"),
                                    .replaceable = &lushGroundReplaceable(),
                                    .minDepth = 3,
                                    .maxDepth = 3,
                                    .extraBottomBlockChance = 0.8F,
                                    .verticalRange = 5,
                                    .vegetationChance = 0.1F,
                                    .extraEdgeColumnChance = 0.7F,
                                    .inwards = -1,
                                    .vegetation = Vegetation::Dripleaf,
                                    .waterlogged = true};
    return config;
}

[[nodiscard]] std::int32_t blockAt(const Chunk& chunk, const DecorationPos& position) {
    return chunk.decorationBlockStateId(position.x, position.y, position.z);
}

void set(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId);
}

[[nodiscard]] std::optional<DecorationPos> findGround(const Chunk& chunk, const DecorationPos& origin,
                                                      const PatchConfig& config) {
    const ChunkHeight bounds = chunk.verticalBounds();
    DecorationPos position = origin;
    for (std::int32_t offset = 0; registry::blocks::isAir(blockAt(chunk, position)) && offset < config.verticalRange;
         ++offset) {
        position.y += config.inwards;
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return std::nullopt;
        }
    }
    for (std::int32_t offset = 0; !registry::blocks::isAir(blockAt(chunk, position)) && offset < config.verticalRange;
         ++offset) {
        position.y -= config.inwards;
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return std::nullopt;
        }
    }
    const DecorationPos ground{.x = position.x, .y = position.y + config.inwards, .z = position.z};
    if (!registry::blocks::isAir(blockAt(chunk, position))) {
        return std::nullopt;
    }
    const registry::BlockFace supportFace = config.inwards < 0 ? registry::BlockFace::Up : registry::BlockFace::Down;
    if (!registry::blockShapes::isFaceSturdy(blockAt(chunk, ground), supportFace)) {
        return std::nullopt;
    }
    return ground;
}

[[nodiscard]] bool placeGround(Chunk& chunk, const DecorationPos& origin, std::int32_t depth,
                               const PatchConfig& config) {
    DecorationPos position = origin;
    const std::string groundPath = pathOf(config.groundStateId);
    for (std::int32_t index = 0; index < depth; ++index) {
        const std::int32_t current = blockAt(chunk, position);
        if (pathOf(current) == groundPath) {
            continue;
        }
        if (!config.replaceable->contains(pathOf(current))) {
            return index != 0;
        }
        set(chunk, position, config.groundStateId);
        position.y += config.inwards;
    }
    return true;
}

[[nodiscard]] std::vector<DecorationPos> waterlogEnclosedSurface(Chunk& chunk,
                                                                 const std::vector<DecorationPos>& surface) {
    std::vector<DecorationPos> enclosed;
    for (const DecorationPos& position : surface) {
        const bool exposed = std::ranges::any_of(kExposureDirections, [&](registry::BlockFace face) {
            const DecorationPos neighbour{.x = position.x + registry::stepX(face),
                                          .y = position.y + registry::stepY(face),
                                          .z = position.z + registry::stepZ(face)};
            return !registry::blockShapes::isFaceSturdy(blockAt(chunk, neighbour), registry::opposite(face));
        });
        if (!exposed) {
            enclosed.push_back(position);
        }
    }
    for (const DecorationPos& position : enclosed) {
        set(chunk, position, registry::blocks::water().stateId);
    }
    return enclosed;
}

void placeMossVegetation(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t roll = random.nextInt(96);
    std::int32_t stateId = state("tall_grass");
    if (roll < 4) {
        stateId = state("flowering_azalea");
    } else if (roll < 11) {
        stateId = state("azalea");
    } else if (roll < 36) {
        stateId = state("moss_carpet");
    } else if (roll < 86) {
        stateId = state("short_grass");
    }
    const SimpleBlockFeature feature{[stateId](random::RandomSource&, const DecorationPos&) { return stateId; },
                                     stateId == state("tall_grass")};
    feature.place(chunk, random, origin);
}

[[nodiscard]] bool hasSourceWaterFluid(std::int32_t stateId) {
    const std::int32_t water = registry::blocks::water().stateId;
    return registry::blockFluids::hasWaterFluid(stateId) && !(stateId > water && stateId <= water + 15);
}

[[nodiscard]] bool canReplaceWithDripleaf(std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || pathOf(stateId) == "water";
}

[[nodiscard]] std::int32_t stateIdWith(std::string_view block, std::map<std::string, std::string> properties) {
    const std::optional<std::int32_t> resolved =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{block}), properties);
    if (!resolved.has_value()) {
        throw std::runtime_error{"unknown " + std::string{block} + " state"};
    }
    return *resolved;
}

[[nodiscard]] bool placeSmallDripleaf(Chunk& chunk, const DecorationPos& origin, std::string_view facing) {
    const std::int32_t current = blockAt(chunk, origin);
    const std::int32_t below = blockAt(chunk, DecorationPos{.x = origin.x, .y = origin.y - 1, .z = origin.z});
    const std::string belowPath = pathOf(below);
    const bool survives = supportsSmallDripleafPaths().contains(belowPath) ||
                          (hasSourceWaterFluid(current) && supportsVegetationPaths().contains(belowPath));
    if (!survives) {
        return false;
    }
    const DecorationPos upperOrigin{.x = origin.x, .y = origin.y + 1, .z = origin.z};
    const std::int32_t upperCurrent = blockAt(chunk, upperOrigin);
    if (!registry::blocks::isAir(upperCurrent)) {
        return false;
    }
    set(chunk, origin,
        stateIdWith("small_dripleaf",
                    {{"facing", std::string{facing}},
                     {"half", "lower"},
                     {"waterlogged", registry::blockFluids::hasWaterFluid(current) ? "true" : "false"}}));
    set(chunk, upperOrigin,
        stateIdWith("small_dripleaf",
                    {{"facing", std::string{facing}},
                     {"half", "upper"},
                     {"waterlogged", registry::blockFluids::hasWaterFluid(upperCurrent) ? "true" : "false"}}));
    return true;
}

bool placeBigDripleaf(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                      std::string_view facing) {
    std::int32_t stemHeight = random.nextInt(3) < 2 ? random.nextInt(5) : 0;
    std::int32_t tipHeight = 1;
    const std::int32_t totalHeight = stemHeight + tipHeight;
    std::int32_t validHeight = totalHeight;
    DecorationPos next{.x = origin.x, .y = origin.y + 1, .z = origin.z};
    for (std::int32_t distance = 0; distance < totalHeight; ++distance) {
        if (!canReplaceWithDripleaf(blockAt(chunk, next))) {
            validHeight = distance;
            break;
        }
        next.y++;
    }
    std::int32_t remove = totalHeight - validHeight;
    const std::int32_t removedStems = std::min(stemHeight, remove);
    stemHeight -= removedStems;
    remove -= removedStems;
    tipHeight -= std::min(tipHeight, remove);

    DecorationPos position = origin;
    for (std::int32_t index = 0; index < stemHeight; ++index) {
        set(chunk, position,
            stateIdWith("big_dripleaf_stem", {{"facing", std::string{facing}}, {"waterlogged", "false"}}));
        position.y++;
    }
    for (std::int32_t index = 0; index < tipHeight; ++index) {
        set(chunk, position,
            stateIdWith("big_dripleaf", {{"facing", std::string{facing}}, {"tilt", "none"}, {"waterlogged", "false"}}));
        position.y++;
    }
    return true;
}

void waterlogPlacedVegetation(Chunk& chunk, const DecorationPos& origin) {
    const std::int32_t stateId = blockAt(chunk, origin);
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto waterlogged = properties.find("waterlogged");
    if (waterlogged == properties.end() || waterlogged->second != "false") {
        return;
    }
    waterlogged->second = "true";
    const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(*block, properties);
    if (resolved.has_value()) {
        set(chunk, origin, *resolved);
    }
}

bool placePatch(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, const PatchConfig& config) {
    // parity: VegetationPatchFeature adds one to each independently sampled UniformInt(4..7).
    const std::int32_t xRadius = 5 + random.nextInt(4);
    const std::int32_t zRadius = 5 + random.nextInt(4);
    std::vector<DecorationPos> surface;
    surface.reserve(static_cast<std::size_t>((xRadius * 2 + 1) * (zRadius * 2 + 1)));

    for (std::int32_t dx = -xRadius; dx <= xRadius; ++dx) {
        const bool xEdge = dx == -xRadius || dx == xRadius;
        for (std::int32_t dz = -zRadius; dz <= zRadius; ++dz) {
            const bool zEdge = dz == -zRadius || dz == zRadius;
            if (xEdge && zEdge) {
                continue;
            }
            if ((xEdge || zEdge) && random.nextFloat() > config.extraEdgeColumnChance) {
                continue;
            }
            const DecorationPos candidate{.x = origin.x + dx, .y = origin.y, .z = origin.z + dz};
            const std::optional<DecorationPos> ground = findGround(chunk, candidate, config);
            if (!ground.has_value()) {
                continue;
            }
            const std::int32_t extraDepth =
                config.maxDepth == config.minDepth ? 0 : random.nextInt(config.maxDepth - config.minDepth + 1);
            std::int32_t depth = config.minDepth + extraDepth;
            if (config.extraBottomBlockChance > 0.0F && random.nextFloat() < config.extraBottomBlockChance) {
                depth++;
            }
            if (placeGround(chunk, *ground, depth, config)) {
                surface.push_back(*ground);
            }
        }
    }

    const std::vector<DecorationPos> vegetationSurface =
        config.waterlogged ? waterlogEnclosedSurface(chunk, surface) : surface;
    for (const DecorationPos& ground : javaHashSetOrder(vegetationSurface)) {
        // parity: vanilla short-circuits on a zero chance and makes no draw.
        if (config.vegetationChance <= 0.0F || random.nextFloat() >= config.vegetationChance) {
            continue;
        }
        const DecorationPos vegetationOrigin =
            config.waterlogged ? ground : DecorationPos{.x = ground.x, .y = ground.y - config.inwards, .z = ground.z};
        switch (config.vegetation) {
        case Vegetation::CaveVine:
            caveVineColumnFeature::place(chunk, random, vegetationOrigin, true);
            break;
        case Vegetation::Moss:
            placeMossVegetation(chunk, random, vegetationOrigin);
            break;
        case Vegetation::Dripleaf: {
            const bool placed = lushVegetationPatchFeature::placeDripleafAt(chunk, random, vegetationOrigin);
            if (config.waterlogged && placed) {
                waterlogPlacedVegetation(chunk, vegetationOrigin);
            }
            break;
        }
        }
    }
    return !surface.empty();
}

} // namespace

bool lushVegetationPatchFeature::placeDripleafAt(Chunk& chunk, random::RandomSource& random,
                                                 const DecorationPos& origin) {
    const std::int32_t selected = random.nextInt(5);
    if (selected == 0) {
        return placeSmallDripleaf(chunk, origin,
                                  kFacingNames[static_cast<std::size_t>(random.nextInt(kFacingNames.size()))]);
    }
    return placeBigDripleaf(chunk, random, origin, kBigDripleafFacingNames[static_cast<std::size_t>(selected - 1)]);
}

bool lushVegetationPatchFeature::placeCeilingMoss(Chunk& chunk, random::RandomSource& random,
                                                  const DecorationPos& origin) {
    return placePatch(chunk, random, origin, ceilingMoss());
}

bool lushVegetationPatchFeature::placeFloorMoss(Chunk& chunk, random::RandomSource& random,
                                                const DecorationPos& origin) {
    return placePatch(chunk, random, origin, floorMoss());
}

bool lushVegetationPatchFeature::placeClay(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    return placePatch(chunk, random, origin, random.nextBoolean() ? clay() : waterloggedClay());
}

} // namespace basalt::server::world::terrain::decoration

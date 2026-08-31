#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

using PathSet = std::set<std::string, std::less<>>;

const PathSet& vegetationPaths() {
    static const PathSet paths = {"short_grass",
                                  "fern",
                                  "tall_grass",
                                  "large_fern",
                                  "bush",
                                  "firefly_bush",
                                  "sweet_berry_bush",
                                  "dandelion",
                                  "poppy",
                                  "blue_orchid",
                                  "allium",
                                  "azure_bluet",
                                  "red_tulip",
                                  "orange_tulip",
                                  "white_tulip",
                                  "pink_tulip",
                                  "oxeye_daisy",
                                  "cornflower",
                                  "lily_of_the_valley",
                                  "closed_eyeblossom",
                                  "open_eyeblossom",
                                  "sunflower",
                                  "lilac",
                                  "rose_bush",
                                  "peony",
                                  "pink_petals",
                                  "wildflowers"};
    return paths;
}

const PathSet& dryVegetationPaths() {
    static const PathSet paths = {"dead_bush", "short_dry_grass", "tall_dry_grass"};
    return paths;
}

const PathSet& supportsLilyPadPaths() {
    static const PathSet paths = {"ice", "frosted_ice"};
    return paths;
}

const PathSet& overridesMushroomLightPaths() {
    static const PathSet paths = {"mycelium", "podzol", "crimson_nylium", "warped_nylium"};
    return paths;
}

const PathSet& supportsVegetationPaths() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt", "mud",    "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "grass_block", "podzol", "mycelium",
                                  "farmland"};
    return paths;
}

const PathSet& supportsDryVegetationPaths() {
    static const PathSet paths = [] {
        PathSet built = supportsVegetationPaths();
        for (const std::string_view extra : {"sand",
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
                                             "black_terracotta"}) {
            built.insert(std::string{extra});
        }
        return built;
    }();
    return paths;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] bool contains(const PathSet& paths, const std::string& path) {
    return !path.empty() && paths.find(path) != paths.end();
}

[[nodiscard]] bool canSurvive(std::int32_t stateId, std::int32_t belowStateId) {
    const std::string path = pathOf(stateId);
    const std::string belowPath = pathOf(belowStateId);
    if (path == "pale_moss_carpet") {
        return !registry::blocks::isAir(belowStateId);
    }
    if (path == "lily_pad") {
        return registry::blockFluids::hasWaterFluid(belowStateId) || contains(supportsLilyPadPaths(), belowPath);
    }
    if (path == "brown_mushroom" || path == "red_mushroom") {
        return contains(overridesMushroomLightPaths(), belowPath) || registry::blockShapes::isSolidRender(belowStateId);
    }
    if (contains(dryVegetationPaths(), path)) {
        return contains(supportsDryVegetationPaths(), belowPath);
    }
    if (contains(vegetationPaths(), path)) {
        return contains(supportsVegetationPaths(), belowPath);
    }
    if (path == "leaf_litter") {
        return registry::blockShapes::isFaceSturdy(belowStateId, registry::BlockFace::Up);
    }
    return heightmapMatches(HeightmapType::OceanFloor, belowStateId);
}

struct HorizontalDirection {
    registry::BlockFace face;
    std::string_view property;
};

constexpr std::array<HorizontalDirection, 4> kHorizontalDirections = {{{registry::BlockFace::North, "north"},
                                                                       {registry::BlockFace::East, "east"},
                                                                       {registry::BlockFace::South, "south"},
                                                                       {registry::BlockFace::West, "west"}}};

[[nodiscard]] const wire::common::Identifier& paleMossCarpetId() {
    static const wire::common::Identifier id = wire::common::Identifier::vanilla("pale_moss_carpet");
    return id;
}

[[nodiscard]] std::int32_t paleMossState(const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId = registry::blocks::stateIdOf(paleMossCarpetId(), properties);
    if (!stateId.has_value()) {
        throw std::runtime_error{"unknown pale moss carpet state"};
    }
    return *stateId;
}

[[nodiscard]] bool isPaleMossCarpet(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && *block == paleMossCarpetId();
}

[[nodiscard]] bool canSupportPaleMossAtFace(const Chunk& chunk, const DecorationPos& origin, registry::BlockFace face) {
    const std::int32_t neighbour = chunk.decorationBlockStateId(
        origin.x + registry::stepX(face), origin.y + registry::stepY(face), origin.z + registry::stepZ(face));
    const registry::BlockFace attachmentFace = registry::opposite(face);
    return registry::blockShapes::isFaceSturdy(neighbour, attachmentFace) ||
           registry::blockShapes::faceCovers(neighbour, attachmentFace, 0, registry::blockShapes::kUnits, 0,
                                             registry::blockShapes::kUnits);
}

[[nodiscard]] std::int32_t updatedPaleMossState(std::int32_t stateId, const Chunk& chunk, const DecorationPos& origin,
                                                bool createSides) {
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const bool isBase = properties["bottom"] == "true";
    const bool shouldCreateSides = createSides || isBase;
    std::optional<std::map<std::string, std::string>> aboveProperties;
    std::optional<std::map<std::string, std::string>> belowProperties;

    for (const HorizontalDirection& direction : kHorizontalDirections) {
        const std::string property{direction.property};
        std::string side;
        if (canSupportPaleMossAtFace(chunk, origin, direction.face)) {
            side = shouldCreateSides ? "low" : properties.at(property);
        } else {
            side = "none";
        }
        if (side == "low") {
            if (!aboveProperties.has_value()) {
                const std::int32_t above = chunk.decorationBlockStateId(origin.x, origin.y + 1, origin.z);
                aboveProperties = isPaleMossCarpet(above) ? registry::blockStateDefinitions::propertiesOf(above)
                                                          : std::map<std::string, std::string>{};
            }
            const auto aboveSide = aboveProperties->find(property);
            const auto aboveBottom = aboveProperties->find("bottom");
            const bool aboveHasSide = aboveSide != aboveProperties->end() && aboveSide->second != "none";
            const bool aboveIsTopper = aboveBottom != aboveProperties->end() && aboveBottom->second == "false";
            if (aboveHasSide && aboveIsTopper) {
                side = "tall";
            }
            if (!isBase) {
                if (!belowProperties.has_value()) {
                    const std::int32_t below = chunk.decorationBlockStateId(origin.x, origin.y - 1, origin.z);
                    belowProperties = isPaleMossCarpet(below) ? registry::blockStateDefinitions::propertiesOf(below)
                                                              : std::map<std::string, std::string>{};
                }
                const auto belowSide = belowProperties->find(property);
                if (belowSide != belowProperties->end() && belowSide->second == "none") {
                    side = "none";
                }
            }
        }
        properties[property] = side;
    }
    return paleMossState(properties);
}

[[nodiscard]] std::optional<std::int32_t> createPaleMossTopper(const Chunk& chunk, random::RandomSource& random,
                                                               const DecorationPos& origin) {
    const DecorationPos above{origin.x, origin.y + 1, origin.z};
    const std::int32_t previous = chunk.decorationBlockStateId(above.x, above.y, above.z);
    const bool previousIsCarpet = isPaleMossCarpet(previous);
    bool previousIsBase = false;
    if (previousIsCarpet) {
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(previous);
        const auto bottom = properties.find("bottom");
        previousIsBase = bottom != properties.end() && bottom->second == "true";
    }
    if ((previousIsCarpet && previousIsBase) ||
        (!previousIsCarpet && !registry::blockShapes::isReplaceable(previous))) {
        return std::nullopt;
    }

    const std::int32_t topperDefault = paleMossState(std::map<std::string, std::string>{{"bottom", "false"}});
    std::int32_t topper = updatedPaleMossState(topperDefault, chunk, above, true);
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(topper);
    for (const HorizontalDirection& direction : kHorizontalDirections) {
        const std::string property{direction.property};
        if (properties.at(property) == "none" || random.nextBoolean()) {
            continue;
        }
        properties[property] = "none";
        topper = paleMossState(properties);
    }
    const bool hasFaces = std::ranges::any_of(kHorizontalDirections, [&properties](const HorizontalDirection& d) {
        return properties.at(std::string{d.property}) != "none";
    });
    if (hasFaces && topper != previous) {
        return topper;
    }
    return std::nullopt;
}

/// parity: `MossyCarpetBlock.placeAt`, including its optional wall-only upper layer.
void placePaleMossCarpet(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t carpetDefault = registry::blocks::named("pale_moss_carpet")->stateId;
    const std::int32_t bottom = updatedPaleMossState(carpetDefault, chunk, origin, true);
    chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, bottom);

    const std::optional<std::int32_t> topper = createPaleMossTopper(chunk, random, origin);
    if (!topper.has_value()) {
        return;
    }
    chunk.setDecorationBlockStateId(origin.x, origin.y + 1, origin.z, *topper);
    const std::int32_t updatedBottom = updatedPaleMossState(bottom, chunk, origin, true);
    chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, updatedBottom);
}

} // namespace

bool SimpleBlockFeature::place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                               random::RandomSource& mossyCarpetRandom) const {
    // parity: vanilla asks the state provider before evaluating canSurvive, so weighted and noise
    // providers consume random even when the candidate ultimately cannot be placed.
    const std::int32_t stateId = toPlace_(random, origin);
    const ChunkHeight bounds = chunk.verticalBounds();
    if (origin.y < bounds.minY() || origin.y > bounds.maxY()) {
        return false;
    }
    if (!registry::blocks::isAir(chunk.decorationBlockStateId(origin.x, origin.y, origin.z))) {
        return false;
    }
    if (origin.y - 1 < bounds.minY()) {
        return false;
    }
    const std::int32_t belowStateId = chunk.decorationBlockStateId(origin.x, origin.y - 1, origin.z);
    if (!canSurvive(stateId, belowStateId)) {
        return false;
    }
    if (isDoublePlant_) {
        if (origin.y + 1 > bounds.maxY() ||
            !registry::blocks::isAir(chunk.decorationBlockStateId(origin.x, origin.y + 1, origin.z))) {
            return false;
        }
    }

    if (isPaleMossCarpet(stateId)) {
        placePaleMossCarpet(chunk, mossyCarpetRandom, origin);
    } else {
        chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, stateId);
        if (isDoublePlant_) {
            const std::optional<std::int32_t> upperStateId =
                registry::blockStateFamilies::upperDoublePlantStateId(stateId);
            if (!upperStateId.has_value()) {
                throw std::runtime_error{"state is not a lower double plant"};
            }
            chunk.setDecorationBlockStateId(origin.x, origin.y + 1, origin.z, *upperStateId);
        }
    }
    if (scheduleTick_) {
        const std::int32_t placedStateId = chunk.decorationBlockStateId(origin.x, origin.y, origin.z);
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(placedStateId);
        if (block == nullptr) {
            return true;
        }
        chunk.scheduleDecorationBlockTick(origin.x, origin.y, origin.z, placedStateId, *block, 1);
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

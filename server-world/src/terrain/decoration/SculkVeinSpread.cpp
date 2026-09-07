#include "SculkShared.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace basalt::server::world::terrain::decoration::sculk {
namespace {

/// parity: `MultifaceBlock.VALID_DIRECTIONS`, which is not the plain direction order.
const std::array<const Direction*, 6> kValidDirections{&kDirections[1], &kDirections[0], &kDirections[2],
                                                       &kDirections[5], &kDirections[3], &kDirections[4]};

constexpr std::array<SpreadType, 3> kAllSpreadTypes{SpreadType::SamePosition, SpreadType::SamePlane,
                                                    SpreadType::WrapAround};
constexpr std::array<SpreadType, 1> kSamePositionOnly{SpreadType::SamePosition};

[[nodiscard]] std::int32_t defaultStateId(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

const wire::common::Identifier& veinId() {
    static const wire::common::Identifier id = wire::common::Identifier::vanilla("sculk_vein");
    return id;
}

void spreadFromFaceTowardRandomDirection(SculkRegion& region, random::RandomSource& random, const DecorationPos& source,
                                         const Direction& startingFace, std::int32_t sourceStateId) {
    std::vector<const Direction*> directions;
    directions.reserve(kDirections.size());
    for (const Direction& direction : kDirections) {
        directions.push_back(&direction);
    }
    shuffle(directions, random);
    for (const Direction* spreadDirection : directions) {
        if (spreadDirection->axis == startingFace.axis) {
            continue;
        }
        if (!SculkRegion::hasFace(sourceStateId, startingFace) ||
            SculkRegion::hasFace(sourceStateId, *spreadDirection)) {
            continue;
        }
        for (const SpreadType type : kAllSpreadTypes) {
            const SpreadPosition spread = spreadPosition(source, *spreadDirection, startingFace, type);
            if (!canSpreadInto(region, source, spread)) {
                continue;
            }
            const std::optional<std::int32_t> oldStateId = region.get(spread.position);
            if (!oldStateId.has_value()) {
                continue;
            }
            const std::optional<std::int32_t> newStateId = region.withFace(*oldStateId, spread.position, *spread.face);
            if (!newStateId.has_value()) {
                continue;
            }
            region.set(spread.position, *newStateId);
            return;
        }
    }
}

bool placeGrowthIfPossible(SculkRegion& region, random::RandomSource& random, const DecorationPos& position,
                           const std::vector<const Direction*>& placementDirections) {
    const std::optional<std::int32_t> oldStateId = region.get(position);
    if (!oldStateId.has_value()) {
        return false;
    }
    for (const Direction* placementDirection : placementDirections) {
        if (!veinSupports().contains(region.blockPath(offsetBy(position, *placementDirection)))) {
            continue;
        }
        const std::optional<std::int32_t> newStateId = region.withFace(*oldStateId, position, *placementDirection);
        if (!newStateId.has_value()) {
            return false;
        }
        region.set(position, *newStateId);
        // parity: the configured feature's spread chance is 1.0, but the draw still happens.
        if (random.nextFloat() < 1.0F) {
            spreadFromFaceTowardRandomDirection(region, random, position, *placementDirection, *newStateId);
        }
        return true;
    }
    return false;
}

} // namespace

const SculkStates& states() {
    static const SculkStates resolved = [] {
        const std::int32_t vein = defaultStateId("sculk_vein");
        const std::int32_t sensor = defaultStateId("sculk_sensor");
        const std::int32_t shrieker = defaultStateId("sculk_shrieker") - 4;
        return SculkStates{.sculk = defaultStateId("sculk"),
                           .vein = vein,
                           .veinWaterlogged = vein - 2,
                           .catalyst = defaultStateId("sculk_catalyst"),
                           .sensor = sensor,
                           .sensorWaterlogged = sensor - 1,
                           .shrieker = shrieker,
                           .shriekerWaterlogged = shrieker - 1};
    }();
    return resolved;
}

const PathSet& veinSupports() {
    static const PathSet paths = {"stone",           "andesite", "diorite", "granite",
                                  "dripstone_block", "calcite",  "tuff",    "deepslate"};
    return paths;
}

const PathSet& spreadForbiddenSupports() {
    static const PathSet paths = {"sculk", "sculk_catalyst", "moving_piston"};
    return paths;
}

const PathSet& nonSturdyBlocks() {
    static const PathSet paths = {"sculk_vein",        "sculk_sensor", "sculk_shrieker",
                                  "pointed_dripstone", "sulfur_spike", "snow"};
    return paths;
}

const PathSet& replaceable() {
    static const PathSet paths = {"stone",
                                  "granite",
                                  "diorite",
                                  "andesite",
                                  "tuff",
                                  "deepslate",
                                  "dirt",
                                  "coarse_dirt",
                                  "rooted_dirt",
                                  "mud",
                                  "muddy_mangrove_roots",
                                  "moss_block",
                                  "pale_moss_block",
                                  "grass_block",
                                  "podzol",
                                  "mycelium",
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
                                  "black_terracotta",
                                  "crimson_nylium",
                                  "warped_nylium",
                                  "netherrack",
                                  "basalt",
                                  "blackstone",
                                  "sand",
                                  "red_sand",
                                  "gravel",
                                  "soul_sand",
                                  "soul_soil",
                                  "calcite",
                                  "smooth_basalt",
                                  "clay",
                                  "dripstone_block",
                                  "end_stone",
                                  "red_sandstone",
                                  "sandstone",
                                  "sulfur",
                                  "cinnabar"};
    return paths;
}

const PathSet& replaceableWorldGen() {
    static const PathSet paths = [] {
        PathSet built = replaceable();
        built.insert({"deepslate_bricks", "deepslate_tiles", "cobbled_deepslate", "cracked_deepslate_bricks",
                      "cracked_deepslate_tiles", "polished_deepslate"});
        return built;
    }();
    return paths;
}

std::string pathOf(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

bool isWaterBlock(const std::int32_t stateId) { return pathOf(stateId) == "water"; }

bool hasSourceWater(const std::int32_t stateId) {
    const std::int32_t water = registry::blocks::water().stateId;
    return registry::blockFluids::hasWaterFluid(stateId) && (stateId < water + 1 || stateId > water + 15);
}

DecorationPos offsetBy(const DecorationPos& position, const Direction& direction) {
    return DecorationPos{.x = position.x + direction.x, .y = position.y + direction.y, .z = position.z + direction.z};
}

std::int32_t manhattanDistance(const DecorationPos& a, const DecorationPos& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
}

std::optional<std::int32_t> SculkRegion::get(const DecorationPos& position) const {
    const ChunkHeight bounds = chunk_->verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
        return chunk_->blockStateId(position.x & 15, position.y, position.z & 15);
    }
    for (const auto& [written, stateId] : externalWrites_) {
        if (written == position) {
            return stateId;
        }
    }
    if (externalBlockState_) {
        if (const std::optional<std::int32_t> observed = externalBlockState_(position.x, position.y, position.z);
            observed.has_value()) {
            return observed;
        }
    }
    return chunk_->decorationBlockStateId(position.x, position.y, position.z);
}

void SculkRegion::set(const DecorationPos& position, const std::int32_t stateId) {
    const ChunkHeight bounds = chunk_->verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return;
    }
    if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
        chunk_->setBlockStateId(position.x & 15, position.y, position.z & 15, stateId);
        return;
    }
    setExternalDecorationBlockStateId(*chunk_, externalWrites_, externalBlockState_, position, stateId);
}

std::string SculkRegion::blockPath(const DecorationPos& position) const {
    const std::optional<std::int32_t> stateId = get(position);
    return stateId.has_value() ? pathOf(*stateId) : std::string{};
}

bool SculkRegion::isAirOrWaterBlock(const DecorationPos& position) const {
    const std::optional<std::int32_t> stateId = get(position);
    return stateId.has_value() && isAirOrWaterBlock(*stateId);
}

bool SculkRegion::isAirOrWaterBlock(const std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || isWaterBlock(stateId);
}

bool SculkRegion::isAirOrWaterFluid(const std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || registry::blockFluids::hasWaterFluid(stateId);
}

bool SculkRegion::isVein(const std::int32_t stateId) { return pathOf(stateId) == "sculk_vein"; }

SculkBehaviour SculkRegion::behaviour(const std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    if (path == "sculk") {
        return SculkBehaviour::Sculk;
    }
    if (path == "sculk_vein") {
        return SculkBehaviour::Vein;
    }
    return SculkBehaviour::Default;
}

bool SculkRegion::canSpreadFrom(const DecorationPos& position) const {
    const std::optional<std::int32_t> stateId = get(position);
    if (!stateId.has_value()) {
        return false;
    }
    if (behaviour(*stateId) != SculkBehaviour::Default) {
        return true;
    }
    if (!registry::blocks::isAir(*stateId) && *stateId != registry::blocks::water().stateId) {
        return false;
    }
    return std::ranges::any_of(
        kDirections, [&](const Direction& direction) { return hasFullCollision(offsetBy(position, direction)); });
}

bool SculkRegion::isReplaceableForVein(const std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || isWaterBlock(stateId) || isVein(stateId);
}

bool SculkRegion::hasSturdyFace(const DecorationPos& position, const Direction& face) const {
    const std::string path = blockPath(position);
    if (path == "sculk_sensor" || path == "sculk_shrieker") {
        return face == kDirections[0];
    }
    if (nonSturdyBlocks().contains(path)) {
        return false;
    }
    const std::optional<std::int32_t> stateId = get(position);
    return stateId.has_value() && heightmapMatches(HeightmapType::OceanFloor, *stateId);
}

bool SculkRegion::hasFullCollision(const DecorationPos& position) const {
    if (nonSturdyBlocks().contains(blockPath(position))) {
        return false;
    }
    const std::optional<std::int32_t> stateId = get(position);
    return stateId.has_value() && heightmapMatches(HeightmapType::OceanFloor, *stateId);
}

bool SculkRegion::hasFace(const std::int32_t stateId, const Direction& direction) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(std::string{direction.property});
    return found != properties.end() && found->second == "true";
}

std::optional<std::int32_t> SculkRegion::withFace(const std::int32_t oldStateId, const DecorationPos& position,
                                                  const Direction& direction) const {
    if (!hasSturdyFace(offsetBy(position, direction), opposite(direction))) {
        return std::nullopt;
    }
    std::map<std::string, std::string> properties;
    if (isVein(oldStateId)) {
        properties = registry::blockStateDefinitions::propertiesOf(oldStateId);
    } else {
        for (const Direction& face : kDirections) {
            properties.emplace(std::string{face.property}, "false");
        }
        properties["waterlogged"] = hasSourceWater(oldStateId) ? "true" : "false";
    }
    const auto found = properties.find(std::string{direction.property});
    if (found != properties.end() && found->second == "true") {
        return std::nullopt;
    }
    properties[std::string{direction.property}] = "true";
    return registry::blocks::stateIdOf(veinId(), properties);
}

SpreadPosition spreadPosition(const DecorationPos& source, const Direction& spreadDirection,
                              const Direction& startingFace, const SpreadType type) {
    switch (type) {
    case SpreadType::SamePosition:
        return SpreadPosition{.position = source, .face = &spreadDirection};
    case SpreadType::SamePlane:
        return SpreadPosition{.position = offsetBy(source, spreadDirection), .face = &startingFace};
    case SpreadType::WrapAround:
    default:
        return SpreadPosition{.position = offsetBy(offsetBy(source, spreadDirection), startingFace),
                              .face = &opposite(spreadDirection)};
    }
}

bool canSpreadInto(const SculkRegion& region, const DecorationPos& source, const SpreadPosition& spread) {
    const std::optional<std::int32_t> existingStateId = region.get(spread.position);
    if (!existingStateId.has_value()) {
        return false;
    }
    const DecorationPos against = offsetBy(spread.position, *spread.face);
    if (spreadForbiddenSupports().contains(region.blockPath(against))) {
        return false;
    }
    if (manhattanDistance(source, spread.position) == 2) {
        const DecorationPos neighbour = offsetBy(source, opposite(*spread.face));
        if (region.hasSturdyFace(neighbour, *spread.face)) {
            return false;
        }
    }
    if (registry::blockFluids::hasFluid(*existingStateId) && !registry::blockFluids::hasWaterFluid(*existingStateId)) {
        return false;
    }
    if (!SculkRegion::isReplaceableForVein(*existingStateId)) {
        return false;
    }
    if (SculkRegion::isVein(*existingStateId) && SculkRegion::hasFace(*existingStateId, *spread.face)) {
        return false;
    }
    return region.hasSturdyFace(against, opposite(*spread.face));
}

std::int32_t spreadAll(SculkRegion& region, const std::int32_t sourceStateId, const DecorationPos& source,
                       const bool sameSpaceOnly) {
    std::int32_t placements = 0;
    const bool otherBlockSource = !SculkRegion::isVein(sourceStateId);
    for (const Direction& startingFace : kDirections) {
        if (!otherBlockSource && !SculkRegion::hasFace(sourceStateId, startingFace)) {
            continue;
        }
        for (const Direction& spreadDirection : kDirections) {
            if (spreadDirection.axis == startingFace.axis) {
                continue;
            }
            if (!otherBlockSource && SculkRegion::hasFace(sourceStateId, spreadDirection)) {
                continue;
            }
            const std::span<const SpreadType> spreadTypes = sameSpaceOnly
                                                                ? std::span<const SpreadType>{kSamePositionOnly}
                                                                : std::span<const SpreadType>{kAllSpreadTypes};
            for (const SpreadType type : spreadTypes) {
                const SpreadPosition spread = spreadPosition(source, spreadDirection, startingFace, type);
                if (!canSpreadInto(region, source, spread)) {
                    continue;
                }
                const std::optional<std::int32_t> oldStateId = region.get(spread.position);
                if (!oldStateId.has_value()) {
                    continue;
                }
                const std::optional<std::int32_t> newStateId =
                    region.withFace(*oldStateId, spread.position, *spread.face);
                if (!newStateId.has_value()) {
                    continue;
                }
                region.set(spread.position, *newStateId);
                ++placements;
                break;
            }
        }
    }
    return placements;
}

void dischargeVein(SculkRegion& region, const DecorationPos& position, const std::int32_t initialStateId) {
    std::int32_t stateId = initialStateId;
    for (const Direction& direction : kDirections) {
        if (!SculkRegion::hasFace(stateId, direction)) {
            continue;
        }
        if (region.blockPath(offsetBy(position, direction)) != "sculk") {
            continue;
        }
        std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
        properties[std::string{direction.property}] = "false";
        stateId = registry::blocks::stateIdOf(veinId(), properties).value_or(stateId);
    }
    const std::int32_t settled = stateId;
    if (std::ranges::none_of(
            kDirections, [settled](const Direction& direction) { return SculkRegion::hasFace(settled, direction); })) {
        stateId = registry::blockFluids::hasWaterFluid(initialStateId) ? registry::blocks::water().stateId
                                                                       : registry::blocks::air().stateId;
    }
    region.set(position, stateId);
}

bool placeMultifaceGrowth(SculkRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    if (!region.isAirOrWaterBlock(origin)) {
        return false;
    }
    std::vector<const Direction*> searchDirections{kValidDirections.begin(), kValidDirections.end()};
    shuffle(searchDirections, random);
    if (placeGrowthIfPossible(region, random, origin, searchDirections)) {
        return true;
    }

    for (const Direction* searchDirection : searchDirections) {
        std::vector<const Direction*> placementDirections;
        placementDirections.reserve(kValidDirections.size());
        for (const Direction* candidate : kValidDirections) {
            if (!(*candidate == opposite(*searchDirection))) {
                placementDirections.push_back(candidate);
            }
        }
        shuffle(placementDirections, random);
        // parity: 26.2's MultifaceGrowthFeature resets to origin + direction on every one of its 20
        // iterations, so a failed immutable attempt has the same result twenty times.
        const DecorationPos position = offsetBy(origin, *searchDirection);
        const std::optional<std::int32_t> stateId = region.get(position);
        if (!stateId.has_value()) {
            continue;
        }
        if (!SculkRegion::isAirOrWaterBlock(*stateId) && !SculkRegion::isVein(*stateId)) {
            continue;
        }
        if (placeGrowthIfPossible(region, random, position, placementDirections)) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration::sculk

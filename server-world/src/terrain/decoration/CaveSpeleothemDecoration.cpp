#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveSpeleothemDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/StoneBlockTags.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kUndergroundDecorationStep = 7;
constexpr std::int32_t kSulfurClusterIndex = 2;
constexpr std::int32_t kSulfurSpikeIndex = 3;
constexpr std::int32_t kDripstoneClusterIndex = 4;
constexpr std::int32_t kPointedDripstoneIndex = 5;
constexpr std::int32_t kDown = -1;
constexpr std::int32_t kUp = 1;

using PathSet = std::set<std::string, std::less<>>;

struct Offset {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

constexpr std::array<Offset, 6> kDirections{Offset{0, -1, 0}, Offset{0, 1, 0},  Offset{0, 0, -1},
                                            Offset{0, 0, 1},  Offset{-1, 0, 0}, Offset{1, 0, 0}};
constexpr std::array<Offset, 4> kHorizontalDirections{kDirections[2], kDirections[5], kDirections[3], kDirections[4]};

struct SpeleothemConfig {
    std::int32_t globalIndex;
    std::string_view biome;
    std::int32_t baseStateId;
    std::string_view pointPath;
    const PathSet* replaceable;
};

struct ClusterConfig {
    std::int32_t globalIndex;
    const SpeleothemConfig* speleothem;
    std::int32_t minHeight;
    std::int32_t maxHeight;
    float wetnessMean;
    float wetnessDeviation;
    float minWetness;
    float maxWetness;
};

/// What a scan up and down from one position found: the first non-empty block each way, if any.
struct CaveColumn {
    std::optional<std::int32_t> floor;
    std::optional<std::int32_t> ceiling;

    [[nodiscard]] std::optional<std::int32_t> height() const {
        if (!floor.has_value() || !ceiling.has_value()) {
            return std::nullopt;
        }
        return *ceiling - *floor - 1;
    }
};

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

const PathSet& baseStoneOverworld() { return stoneBlockTags::baseStoneOverworld(); }

const PathSet& sulfurReplaceable() {
    static const PathSet paths = {"sulfur", "cinnabar"};
    return paths;
}

[[nodiscard]] bool isWaterBlock(std::int32_t stateId) { return pathOf(stateId) == "water"; }

[[nodiscard]] DecorationPos offset(const DecorationPos& position, std::int32_t dx, std::int32_t dy, std::int32_t dz) {
    return DecorationPos{.x = position.x + dx, .y = position.y + dy, .z = position.z + dz};
}

[[nodiscard]] DecorationPos withY(const DecorationPos& position, std::int32_t y) {
    return DecorationPos{.x = position.x, .y = y, .z = position.z};
}

/// The read and write view these features work through: a cluster reaches past its own chunk, and
/// the part outside used to be collected and then dropped.
class SpeleothemRegion {
public:
    SpeleothemRegion(Chunk& chunk, ExternalStateReader externalBlockState)
        : chunk_(&chunk), externalBlockState_(std::move(externalBlockState)) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    [[nodiscard]] std::optional<std::int32_t> get(const DecorationPos& position) const {
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

    void set(const DecorationPos& position, std::int32_t stateId) {
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

    [[nodiscard]] std::string blockPath(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() ? pathOf(*stateId) : std::string{};
    }

    [[nodiscard]] bool isEmptyOrWater(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() && (registry::blocks::isAir(*stateId) || isWaterBlock(*stateId));
    }

    [[nodiscard]] bool isSolid(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() && registry::blockShapes::isSolidState(*stateId);
    }

    [[nodiscard]] bool isNeitherEmptyNorWater(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() && !registry::blocks::isAir(*stateId) && !isWaterBlock(*stateId);
    }

    [[nodiscard]] bool isLava(const DecorationPos& position) const { return blockPath(position) == "lava"; }

    [[nodiscard]] bool hasWater(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() && registry::blockFluids::hasWaterFluid(*stateId);
    }

    [[nodiscard]] bool canBeAdjacentToWater(const DecorationPos& position) const {
        return baseStoneOverworld().contains(blockPath(position)) || hasWater(position);
    }

    [[nodiscard]] bool isBase(const DecorationPos& position, const SpeleothemConfig& config) const {
        const std::optional<std::int32_t> stateId = get(position);
        if (!stateId.has_value()) {
            return false;
        }
        return *stateId == config.baseStateId || config.replaceable->contains(pathOf(*stateId));
    }

private:
    Chunk* chunk_;
    ExternalStateReader externalBlockState_;
    std::vector<std::pair<DecorationPos, std::int32_t>> externalWrites_;
};

[[nodiscard]] std::int32_t clampedNormal(random::RandomSource& random, float deviation, std::int32_t minInclusive,
                                         std::int32_t maxInclusive) {
    const float value = static_cast<float>(random.nextGaussian()) * deviation;
    return static_cast<std::int32_t>(
        std::clamp(value, static_cast<float>(minInclusive), static_cast<float>(maxInclusive)));
}

/// parity: a speleothem replaces `#dripstone_replaceable_blocks`, and vanilla decides that against
/// whatever is really at the position. A write that crosses the chunk edge has to carry the same
/// target predicate or it is refused as soon as the neighbour's own decoration touched the cell.
void setSpeleothemBlock(SpeleothemRegion& region, const DecorationPos& position, std::int32_t stateId,
                        const SpeleothemConfig& config) {
    const Chunk::WritePolicyScope scope{region.chunk(), oreBlockRules::policyFor(*config.replaceable)};
    region.set(position, stateId);
}

[[nodiscard]] float clampedNormalFloat(random::RandomSource& random, float mean, float deviation, float minInclusive,
                                       float maxInclusive) {
    return std::clamp(mean + static_cast<float>(random.nextGaussian()) * deviation, minInclusive, maxInclusive);
}

[[nodiscard]] double clampedMap(double value, double fromMin, double fromMax, double toMin, double toMax) {
    const double factor = (value - fromMin) / (fromMax - fromMin);
    if (factor < 0.0) {
        return toMin;
    }
    if (factor > 1.0) {
        return toMax;
    }
    return toMin + factor * (toMax - toMin);
}

[[nodiscard]] float clampedMapFloat(float value, float fromMin, float fromMax, float toMin, float toMax) {
    const float factor = (value - fromMin) / (fromMax - fromMin);
    if (factor < 0.0F) {
        return toMin;
    }
    if (factor > 1.0F) {
        return toMax;
    }
    return toMin + factor * (toMax - toMin);
}

[[nodiscard]] std::int32_t pointedStateId(std::string_view path, std::string_view thickness, std::int32_t tipDirection,
                                          bool waterlogged) {
    const std::map<std::string, std::string> properties{{"thickness", std::string{thickness}},
                                                        {"vertical_direction", tipDirection == kUp ? "up" : "down"},
                                                        {"waterlogged", waterlogged ? "true" : "false"}};
    const std::optional<std::int32_t> resolved =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    if (!resolved.has_value()) {
        throw std::runtime_error{"missing " + std::string{path} + " state"};
    }
    return *resolved;
}

void growSpeleothem(SpeleothemRegion& region, const DecorationPos& origin, std::int32_t tipDirection,
                    std::int32_t height, const SpeleothemConfig& config, bool mergedTip = false) {
    if (!region.isBase(offset(origin, 0, -tipDirection, 0), config)) {
        return;
    }
    DecorationPos position = origin;
    const auto place = [&](std::string_view thickness) {
        setSpeleothemBlock(region, position,
                           pointedStateId(config.pointPath, thickness, tipDirection, region.hasWater(position)),
                           config);
        position = offset(position, 0, tipDirection, 0);
    };
    if (height >= 3) {
        place("base");
        for (std::int32_t middle = 0; middle < height - 3; ++middle) {
            place("middle");
        }
    }
    if (height >= 2) {
        place("frustum");
    }
    if (height >= 1) {
        place(mergedTip ? "tip_merge" : "tip");
    }
}

void placeBaseIfPossible(SpeleothemRegion& region, const DecorationPos& position, const SpeleothemConfig& config) {
    if (config.replaceable->contains(region.blockPath(position))) {
        setSpeleothemBlock(region, position, config.baseStateId, config);
    }
}

bool placeSpeleothem(SpeleothemRegion& region, random::RandomSource& random, const DecorationPos& origin,
                     const SpeleothemConfig& config) {
    const bool canPlaceAbove = region.isBase(offset(origin, 0, 1, 0), config);
    const bool canPlaceBelow = region.isBase(offset(origin, 0, -1, 0), config);
    std::int32_t tipDirection = 0;
    if (canPlaceAbove && canPlaceBelow) {
        tipDirection = random.nextBoolean() ? kDown : kUp;
    } else if (canPlaceAbove) {
        tipDirection = kDown;
    } else if (canPlaceBelow) {
        tipDirection = kUp;
    } else {
        return false;
    }

    const DecorationPos root = offset(origin, 0, -tipDirection, 0);
    placeBaseIfPossible(region, root, config);
    for (const Offset& direction : kHorizontalDirections) {
        if (random.nextFloat() > 0.7F) {
            continue;
        }
        const DecorationPos radius1 = offset(root, direction.x, direction.y, direction.z);
        placeBaseIfPossible(region, radius1, config);
        if (random.nextFloat() > 0.5F) {
            continue;
        }
        const Offset& radius2Direction =
            kDirections[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kDirections.size())))];
        const DecorationPos radius2 = offset(radius1, radius2Direction.x, radius2Direction.y, radius2Direction.z);
        placeBaseIfPossible(region, radius2, config);
        if (random.nextFloat() > 0.5F) {
            continue;
        }
        const Offset& radius3Direction =
            kDirections[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kDirections.size())))];
        placeBaseIfPossible(region, offset(radius2, radius3Direction.x, radius3Direction.y, radius3Direction.z),
                            config);
    }

    const std::int32_t height =
        random.nextFloat() < 0.2F && region.isEmptyOrWater(offset(origin, 0, tipDirection, 0)) ? 2 : 1;
    growSpeleothem(region, origin, tipDirection, height, config);
    return true;
}

[[nodiscard]] std::optional<CaveColumn> scanColumn(const SpeleothemRegion& region, const DecorationPos& origin,
                                                   std::int32_t searchRange) {
    if (!region.isEmptyOrWater(origin)) {
        return std::nullopt;
    }
    const auto scan = [&](std::int32_t direction) -> std::optional<std::int32_t> {
        DecorationPos position = origin;
        for (std::int32_t step = 1; step < searchRange; ++step) {
            if (!region.isEmptyOrWater(position)) {
                break;
            }
            position = offset(position, 0, direction, 0);
        }
        if (region.isNeitherEmptyNorWater(position)) {
            return position.y;
        }
        return std::nullopt;
    };
    return CaveColumn{.floor = scan(kDown), .ceiling = scan(kUp)};
}

[[nodiscard]] bool canPlacePool(const SpeleothemRegion& region, const DecorationPos& floor,
                                const ClusterConfig& config) {
    const std::optional<std::int32_t> stateId = region.get(floor);
    if (!stateId.has_value()) {
        return false;
    }
    const std::string blockPath = pathOf(*stateId);
    if (isWaterBlock(*stateId) || *stateId == config.speleothem->baseStateId ||
        blockPath == config.speleothem->pointPath) {
        return false;
    }
    if (region.hasWater(offset(floor, 0, 1, 0))) {
        return false;
    }
    for (const Offset& direction : kHorizontalDirections) {
        if (!region.canBeAdjacentToWater(offset(floor, direction.x, direction.y, direction.z))) {
            return false;
        }
    }
    return region.canBeAdjacentToWater(offset(floor, 0, -1, 0));
}

void replaceWithBase(SpeleothemRegion& region, const DecorationPos& first, std::int32_t maxCount,
                     std::int32_t direction, const SpeleothemConfig& config) {
    DecorationPos position = first;
    for (std::int32_t step = 0; step < maxCount; ++step) {
        if (!config.replaceable->contains(region.blockPath(position))) {
            return;
        }
        setSpeleothemBlock(region, position, config.baseStateId, config);
        position = offset(position, 0, direction, 0);
    }
}

[[nodiscard]] std::int32_t speleothemHeight(random::RandomSource& random, std::int32_t dx, std::int32_t dz,
                                            float density, std::int32_t maxHeight) {
    if (random.nextFloat() > density) {
        return 0;
    }
    const std::int32_t distanceFromCenter = std::abs(dx) + std::abs(dz);
    const auto heightMean =
        static_cast<float>(clampedMap(static_cast<double>(distanceFromCenter), 0.0, 8.0, maxHeight / 2.0, 0.0));
    return static_cast<std::int32_t>(clampedNormalFloat(random, heightMean, 3.0F, 0.0F, static_cast<float>(maxHeight)));
}

[[nodiscard]] double chanceOfSpeleothem(std::int32_t xRadius, std::int32_t zRadius, std::int32_t dx, std::int32_t dz) {
    const std::int32_t distanceFromEdge = std::min(xRadius - std::abs(dx), zRadius - std::abs(dz));
    return static_cast<double>(clampedMapFloat(static_cast<float>(distanceFromEdge), 0.0F, 3.0F, 0.1F, 1.0F));
}

void placeColumn(SpeleothemRegion& region, random::RandomSource& random, const DecorationPos& origin, std::int32_t dx,
                 std::int32_t dz, float wetness, double chance, std::int32_t clusterHeight, float density,
                 const ClusterConfig& config) {
    const std::optional<CaveColumn> baseColumn = scanColumn(region, origin, 12);
    if (!baseColumn.has_value() || (!baseColumn->ceiling.has_value() && !baseColumn->floor.has_value())) {
        return;
    }
    const std::optional<std::int32_t> ceiling = baseColumn->ceiling;
    const bool wantPool = random.nextFloat() < wetness;
    CaveColumn column = *baseColumn;
    if (wantPool && baseColumn->floor.has_value() && canPlacePool(region, withY(origin, *baseColumn->floor), config)) {
        region.set(withY(origin, *baseColumn->floor), registry::blocks::water().stateId);
        column.floor = *baseColumn->floor - 1;
    }
    const std::optional<std::int32_t> floor = column.floor;

    const bool wantStalactite = random.nextDouble() < chance;
    std::int32_t stalactiteHeight = 0;
    if (ceiling.has_value() && wantStalactite && !region.isLava(withY(origin, *ceiling))) {
        const std::int32_t thickness = 2 + random.nextInt(3);
        replaceWithBase(region, withY(origin, *ceiling), thickness, kUp, *config.speleothem);
        const std::int32_t maxHeight = floor.has_value() ? std::min(clusterHeight, *ceiling - *floor) : clusterHeight;
        stalactiteHeight = speleothemHeight(random, dx, dz, density, maxHeight);
    }

    const bool wantStalagmite = random.nextDouble() < chance;
    std::int32_t stalagmiteHeight = 0;
    if (floor.has_value() && wantStalagmite && !region.isLava(withY(origin, *floor))) {
        const std::int32_t thickness = 2 + random.nextInt(3);
        replaceWithBase(region, withY(origin, *floor), thickness, kDown, *config.speleothem);
        stalagmiteHeight = ceiling.has_value() ? std::max(0, stalactiteHeight + random.nextInt(3) - 1)
                                               : speleothemHeight(random, dx, dz, density, clusterHeight);
    }

    std::int32_t actualStalactiteHeight = stalactiteHeight;
    std::int32_t actualStalagmiteHeight = stalagmiteHeight;
    if (ceiling.has_value() && floor.has_value() && *ceiling - stalactiteHeight <= *floor + stalagmiteHeight) {
        const std::int32_t lowestStalactiteBottom = std::max(*ceiling - stalactiteHeight, *floor + 1);
        const std::int32_t highestStalagmiteTop = std::min(*floor + stalagmiteHeight, *ceiling - 1);
        const std::int32_t actualStalactiteBottom =
            lowestStalactiteBottom + random.nextInt(highestStalagmiteTop + 1 - lowestStalactiteBottom + 1);
        actualStalagmiteHeight = actualStalactiteBottom - 1 - *floor;
        actualStalactiteHeight = *ceiling - actualStalactiteBottom;
    }

    const bool mergeTips = random.nextBoolean() && actualStalactiteHeight > 0 && actualStalagmiteHeight > 0 &&
                           column.height().has_value() &&
                           actualStalactiteHeight + actualStalagmiteHeight == *column.height();
    if (ceiling.has_value()) {
        growSpeleothem(region, withY(origin, *ceiling - 1), kDown, actualStalactiteHeight, *config.speleothem,
                       mergeTips);
    }
    if (floor.has_value()) {
        growSpeleothem(region, withY(origin, *floor + 1), kUp, actualStalagmiteHeight, *config.speleothem, mergeTips);
    }
}

bool placeCluster(SpeleothemRegion& region, random::RandomSource& random, const DecorationPos& origin,
                  const ClusterConfig& config) {
    if (!region.isEmptyOrWater(origin)) {
        return false;
    }
    const std::int32_t height = config.minHeight + random.nextInt(config.maxHeight - config.minHeight + 1);
    const float wetness = config.wetnessDeviation == 0.0F
                              ? config.wetnessMean
                              : clampedNormalFloat(random, config.wetnessMean, config.wetnessDeviation,
                                                   config.minWetness, config.maxWetness);
    const float density = 0.3F + random.nextFloat() * 0.4F;
    const std::int32_t xRadius = 2 + random.nextInt(7);
    const std::int32_t zRadius = 2 + random.nextInt(7);
    for (std::int32_t dx = -xRadius; dx <= xRadius; ++dx) {
        for (std::int32_t dz = -zRadius; dz <= zRadius; ++dz) {
            const double chance = chanceOfSpeleothem(xRadius, zRadius, dx, dz);
            placeColumn(region, random, offset(origin, dx, 0, dz), dx, dz, wetness, chance, height, density, config);
        }
    }
    return true;
}

[[nodiscard]] std::optional<DecorationPos> scanForSolid(const SpeleothemRegion& region, const DecorationPos& origin,
                                                        std::int32_t direction) {
    const ChunkHeight bounds = region.chunk().verticalBounds();
    DecorationPos position = origin;
    if (!region.isEmptyOrWater(position)) {
        return std::nullopt;
    }
    for (std::int32_t step = 0; step < 12; ++step) {
        if (region.isSolid(position)) {
            return position;
        }
        position = offset(position, 0, direction, 0);
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return std::nullopt;
        }
        if (!region.isEmptyOrWater(position)) {
            break;
        }
    }
    return region.isSolid(position) ? std::optional<DecorationPos>{position} : std::nullopt;
}

const SpeleothemConfig& sulfurSpike() {
    static const SpeleothemConfig config{.globalIndex = kSulfurSpikeIndex,
                                         .biome = "sulfur_caves",
                                         .baseStateId = stateIdOf("sulfur"),
                                         .pointPath = "sulfur_spike",
                                         .replaceable = &sulfurReplaceable()};
    return config;
}

const SpeleothemConfig& pointedDripstone() {
    static const SpeleothemConfig config{.globalIndex = kPointedDripstoneIndex,
                                         .biome = "dripstone_caves",
                                         .baseStateId = stateIdOf("dripstone_block"),
                                         .pointPath = "pointed_dripstone",
                                         .replaceable = &baseStoneOverworld()};
    return config;
}

const ClusterConfig& sulfurCluster() {
    static const ClusterConfig config{.globalIndex = kSulfurClusterIndex,
                                      .speleothem = &sulfurSpike(),
                                      .minHeight = 1,
                                      .maxHeight = 4,
                                      .wetnessMean = 0.0F,
                                      .wetnessDeviation = 0.0F,
                                      .minWetness = 0.0F,
                                      .maxWetness = 0.0F};
    return config;
}

const ClusterConfig& dripstoneCluster() {
    static const ClusterConfig config{.globalIndex = kDripstoneClusterIndex,
                                      .speleothem = &pointedDripstone(),
                                      .minHeight = 3,
                                      .maxHeight = 6,
                                      .wetnessMean = 0.1F,
                                      .wetnessDeviation = 0.3F,
                                      .minWetness = 0.1F,
                                      .maxWetness = 0.9F};
    return config;
}

void placePointedFeature(SpeleothemRegion& region, random::RandomSource& random,
                         const surface::SurfaceRuleEnvironment& environment, const SpeleothemConfig& config) {
    const ChunkHeight bounds = region.chunk().verticalBounds();
    const std::int32_t count = 192 + random.nextInt(65);
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{.x = x, .y = bounds.minY() + random.nextInt(321), .z = z};
        const std::int32_t nestedCount = 1 + random.nextInt(5);
        for (std::int32_t nested = 0; nested < nestedCount; ++nested) {
            // parity: `RandomOffsetPlacement` draws x, then y, then z from the same source. As
            // arguments of one call the order is unspecified, so the sequence is pinned here.
            const std::int32_t offsetX = clampedNormal(random, 3.0F, -10, 10);
            const std::int32_t offsetY = clampedNormal(random, 0.6F, -2, 2);
            const std::int32_t offsetZ = clampedNormal(random, 3.0F, -10, 10);
            const DecorationPos target = offset(origin, offsetX, offsetY, offsetZ);
            if (biomePath(environment.biomeAt(target.x, target.y, target.z)) != config.biome) {
                continue;
            }
            const std::int32_t scanDirection = random.nextInt(2) == 0 ? kDown : kUp;
            const std::optional<DecorationPos> solid = scanForSolid(region, target, scanDirection);
            if (!solid.has_value()) {
                continue;
            }
            static_cast<void>(placeSpeleothem(region, random, offset(*solid, 0, -scanDirection, 0), config));
        }
    }
}

void placeClusters(SpeleothemRegion& region, random::RandomSource& random,
                   const surface::SurfaceRuleEnvironment& environment, const ClusterConfig& config) {
    const ChunkHeight bounds = region.chunk().verticalBounds();
    const std::int32_t count = 48 + random.nextInt(49);
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{.x = x, .y = bounds.minY() + random.nextInt(321), .z = z};
        if (biomePath(environment.biomeAt(x, origin.y, z)) != config.speleothem->biome) {
            continue;
        }
        static_cast<void>(placeCluster(region, random, origin, config));
    }
}

} // namespace

const std::set<std::string, std::less<>>& caveSpeleothemDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"pointed_dripstone", "dripstone_cluster", "sulfur_spike",
                                                           "sulfur_spike_cluster"};
    return ids;
}

/// parity: `ChunkGenerator.applyBiomeDecoration` gathers the biomes of the 3x3 of chunks and only
/// runs the features some biome there lists. A feature nobody lists never draws, which matters for
/// the gaussian: its spare outlives a `setFeatureSeed`.
[[nodiscard]] bool biomeInRange(const surface::SurfaceRuleEnvironment& environment, const Chunk& chunk,
                                std::string_view biome) {
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            const std::int32_t baseX = (chunk.chunkX() + dx) * 16;
            const std::int32_t baseZ = (chunk.chunkZ() + dz) * 16;
            for (std::int32_t x = 2; x < 16; x += 4) {
                for (std::int32_t z = 2; z < 16; z += 4) {
                    for (std::int32_t y = bounds.minY() + 2; y <= bounds.maxY(); y += 4) {
                        if (biomePath(environment.biomeAt(baseX + x, y, baseZ + z)) == biome) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void caveSpeleothemDecoration::decorateFeatures(Chunk& chunk, const std::int64_t seed,
                                                const surface::SurfaceRuleEnvironment& environment,
                                                const std::int32_t firstIndex, const std::int32_t lastIndex,
                                                ExternalStateReader externalBlockState) {
    SpeleothemRegion region{chunk, std::move(externalBlockState)};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const bool anySulfur = biomeInRange(environment, chunk, sulfurSpike().biome);
    const bool anyDripstone = biomeInRange(environment, chunk, pointedDripstone().biome);
    for (std::int32_t globalIndex = firstIndex; globalIndex <= lastIndex; ++globalIndex) {
        const bool sulfur = globalIndex == kSulfurClusterIndex || globalIndex == kSulfurSpikeIndex;
        if (sulfur ? !anySulfur : !anyDripstone) {
            continue;
        }
        random::setFeatureSeed(random, decorationSeed, globalIndex, kUndergroundDecorationStep);
        if (globalIndex == kSulfurClusterIndex) {
            placeClusters(region, random, environment, sulfurCluster());
        } else if (globalIndex == kDripstoneClusterIndex) {
            placeClusters(region, random, environment, dripstoneCluster());
        } else if (globalIndex == kSulfurSpikeIndex) {
            placePointedFeature(region, random, environment, sulfurSpike());
        } else if (globalIndex == kPointedDripstoneIndex) {
            placePointedFeature(region, random, environment, pointedDripstone());
        }
    }
}

void caveSpeleothemDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                        const surface::SurfaceRuleEnvironment& environment,
                                        ExternalStateReader externalBlockState) {
    decorateFeatures(chunk, seed, environment, kSulfurClusterIndex, kPointedDripstoneIndex,
                     std::move(externalBlockState));
}

bool caveSpeleothemDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                       const std::string_view placedFeature) {
    SpeleothemRegion region{chunk, nullptr};
    if (placedFeature == "pointed_dripstone") {
        return placeSpeleothem(region, random, origin, pointedDripstone());
    }
    if (placedFeature == "sulfur_spike") {
        return placeSpeleothem(region, random, origin, sulfurSpike());
    }
    if (placedFeature == "dripstone_cluster") {
        return placeCluster(region, random, origin, dripstoneCluster());
    }
    if (placedFeature == "sulfur_spike_cluster") {
        return placeCluster(region, random, origin, sulfurCluster());
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration

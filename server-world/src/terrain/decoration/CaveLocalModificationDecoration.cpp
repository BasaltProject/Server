#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/decoration/CaveLocalModificationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/OverworldBiomePaths.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kLocalModificationsStep = 2;
constexpr std::int32_t kAmethystGeodeIndex = 2;
constexpr std::int32_t kLargeDripstoneIndex = 3;
constexpr std::int32_t kSourceWaterLevel = 0;

using PathSet = std::set<std::string, std::less<>>;

struct Direction {
    std::string_view name;
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

constexpr std::array<Direction, 6> kDirections{Direction{"down", 0, -1, 0},  Direction{"up", 0, 1, 0},
                                               Direction{"north", 0, 0, -1}, Direction{"south", 0, 0, 1},
                                               Direction{"west", -1, 0, 0},  Direction{"east", 1, 0, 0}};

constexpr std::array<std::string_view, 4> kAmethystPlacements{"small_amethyst_bud", "medium_amethyst_bud",
                                                              "large_amethyst_bud", "amethyst_cluster"};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
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

const PathSet& geodeInvalidBlocks() {
    static const PathSet paths = {"bedrock", "water", "lava", "ice", "packed_ice", "blue_ice"};
    return paths;
}

const PathSet& baseStoneOverworld() {
    static const PathSet paths = {"stone", "granite", "diorite", "andesite", "tuff", "deepslate"};
    return paths;
}

[[nodiscard]] DecorationPos offset(const DecorationPos& position, std::int32_t dx, std::int32_t dy, std::int32_t dz) {
    return DecorationPos{.x = position.x + dx, .y = position.y + dy, .z = position.z + dz};
}

[[nodiscard]] DecorationPos offset(const DecorationPos& position, const Direction& direction) {
    return offset(position, direction.x, direction.y, direction.z);
}

[[nodiscard]] DecorationPos withY(const DecorationPos& position, std::int32_t y) {
    return DecorationPos{.x = position.x, .y = y, .z = position.z};
}

[[nodiscard]] double distanceSquared(const DecorationPos& a, const DecorationPos& b) {
    const std::int32_t dx = a.x - b.x;
    const std::int32_t dy = a.y - b.y;
    const std::int32_t dz = a.z - b.z;
    return static_cast<double>(dx * dx + dy * dy + dz * dz);
}

[[nodiscard]] double inverseSqrt(double value) { return 1.0 / std::sqrt(value); }

[[nodiscard]] float uniformFloat(random::RandomSource& random, float min, float max) {
    return min + random.nextFloat() * (max - min);
}

[[nodiscard]] std::optional<std::int32_t> amethystWaterLevel(std::int32_t stateId) {
    if (pathOf(stateId) != "water") {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("level");
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t level = 0;
    const std::string& text = found->second;
    if (std::from_chars(text.data(), text.data() + text.size(), level).ec != std::errc{}) {
        return std::nullopt;
    }
    return level;
}

[[nodiscard]] std::int32_t amethystPlacementState(std::string_view block, std::string_view facing, bool waterlogged) {
    const std::map<std::string, std::string> properties{{"facing", std::string{facing}},
                                                        {"waterlogged", waterlogged ? "true" : "false"}};
    const std::optional<std::int32_t> resolved =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{block}), properties);
    if (!resolved.has_value()) {
        throw std::runtime_error{"missing state for minecraft:" + std::string{block}};
    }
    return *resolved;
}

/// parity: `LargeDripstoneFeature.getDripstoneHeight`, kept in doubles all the way through.
[[nodiscard]] double speleothemHeight(double suppliedDistance, double radius, double scale, double bluntness) {
    const double distance = std::max(suppliedDistance, bluntness);
    const double ratio = distance / radius * 0.384;
    const double part1 = 0.75 * std::pow(ratio, 1.3333333333333333);
    const double part2 = std::pow(ratio, 0.6666666666666666);
    const double part3 = 0.3333333333333333 * std::log(ratio);
    return std::max(scale * (part1 - part2 - part3), 0.0) / 0.384 * radius;
}

/// The read and write view both features work through.
class LocalModificationRegion {
public:
    LocalModificationRegion(Chunk& chunk, ExternalSurfaceHeightReader externalSurfaceHeight,
                            ExternalStateReader externalBlockState)
        : chunk_(&chunk), externalSurfaceHeight_(std::move(externalSurfaceHeight)),
          externalBlockState_(std::move(externalBlockState)) {}

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

    bool safeSet(const DecorationPos& position, std::int32_t stateId) {
        const std::optional<std::int32_t> current = get(position);
        if (!current.has_value() || !oreBlockRules::canReplaceDeferred(GeneratedBlockWritePolicy::Geode, *current)) {
            return false;
        }
        // A neighbour that does not exist yet reads as air, so testing here decides nothing for it.
        // The policy travels with the write and is tested again against whatever really turns up.
        const Chunk::WritePolicyScope scope{*chunk_, GeneratedBlockWritePolicy::Geode};
        set(position, stateId);
        return true;
    }

    [[nodiscard]] std::string blockPath(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() ? pathOf(*stateId) : std::string{};
    }

    [[nodiscard]] bool isAir(const DecorationPos& position) const {
        const std::optional<std::int32_t> stateId = get(position);
        return stateId.has_value() && registry::blocks::isAir(*stateId);
    }

    [[nodiscard]] bool isEmptyOrWater(const DecorationPos& position) const {
        return isAir(position) || blockPath(position) == "water";
    }

    [[nodiscard]] bool isEmptyOrWaterOrLava(const DecorationPos& position) const {
        if (isAir(position)) {
            return true;
        }
        const std::string path = blockPath(position);
        return path == "water" || path == "lava";
    }

    [[nodiscard]] bool isDripstoneEdge(const DecorationPos& position) const {
        const std::string path = blockPath(position);
        return path == "dripstone_block" || path == "lava" || baseStoneOverworld().contains(path);
    }

    [[nodiscard]] bool isCircleMostlyEmbedded(const DecorationPos& center, std::int32_t radius) const {
        if (isEmptyOrWaterOrLava(center)) {
            return false;
        }
        const float angleIncrement = 6.0F / static_cast<float>(radius);
        for (float angle = 0.0F; angle < static_cast<float>(std::numbers::pi * 2.0); angle += angleIncrement) {
            const auto dx = wire::common::toIntLikeJava(
                static_cast<double>(wire::common::mth::cos(angle) * static_cast<float>(radius)));
            const auto dz = wire::common::toIntLikeJava(
                static_cast<double>(wire::common::mth::sin(angle) * static_cast<float>(radius)));
            if (isEmptyOrWaterOrLava(offset(center, dx, 0, dz))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::int32_t worldSurfaceHeight(std::int32_t x, std::int32_t z) const {
        if ((x >> 4) == chunk_->chunkX() && (z >> 4) == chunk_->chunkZ()) {
            // parity: the large dripstone stops at `WORLD_SURFACE_WG`, the frozen map, so a feature
            // that ran earlier in the chunk cannot let the spike grow through it.
            return chunk_->worldgenHeightAt(x & 15, z & 15, HeightmapType::WorldSurface);
        }
        if (externalSurfaceHeight_) {
            if (const std::optional<std::int32_t> observed = externalSurfaceHeight_(x, z); observed.has_value()) {
                return *observed;
            }
        }
        return chunk_->verticalBounds().maxY() + 1;
    }

    void scheduleFluidTick(const DecorationPos& position, const wire::common::Identifier& fluid) {
        if ((position.x >> 4) != chunk_->chunkX() || (position.z >> 4) != chunk_->chunkZ()) {
            return;
        }
        const ChunkHeight bounds = chunk_->verticalBounds();
        if (position.y < bounds.minY() || position.y > bounds.maxY()) {
            return;
        }
        chunk_->scheduleFluidTick(ChunkScheduledTick{
            .type = fluid, .localX = position.x & 15, .y = position.y, .localZ = position.z & 15, .delay = 0});
    }

private:
    Chunk* chunk_;
    ExternalSurfaceHeightReader externalSurfaceHeight_;
    ExternalStateReader externalBlockState_;
    std::vector<std::pair<DecorationPos, std::int32_t>> externalWrites_;
};

struct Wind {
    bool active = false;
    std::int32_t originY = 0;
    float speedX = 0.0F;
    float speedZ = 0.0F;
    std::int32_t maxOffset = 0;

    [[nodiscard]] DecorationPos offsetOf(const DecorationPos& position) const {
        if (!active) {
            return position;
        }
        const std::int32_t dy = originY - position.y;
        const std::int32_t dx =
            std::clamp(wire::common::mth::floor(static_cast<double>(speedX) * dy), -maxOffset, maxOffset);
        const std::int32_t dz =
            std::clamp(wire::common::mth::floor(static_cast<double>(speedZ) * dy), -maxOffset, maxOffset);
        return offset(position, dx, 0, dz);
    }
};

class LargeDripstone {
public:
    LargeDripstone(DecorationPos root, bool pointingUp, std::int32_t radius, double bluntness, double scale)
        : root_(root), pointingUp_(pointingUp), radius_(radius), bluntness_(bluntness), scale_(scale) {}

    [[nodiscard]] bool isSuitableForWind() const { return radius_ >= 4 && bluntness_ >= 0.6; }

    bool embedBase(const LocalModificationRegion& region, const Wind& wind) {
        while (radius_ > 1) {
            DecorationPos newRoot = root_;
            const std::int32_t steps = std::min(10, height());
            for (std::int32_t step = 0; step < steps; ++step) {
                if (region.blockPath(newRoot) == "lava") {
                    return false;
                }
                if (region.isCircleMostlyEmbedded(wind.offsetOf(newRoot), radius_)) {
                    root_ = newRoot;
                    return true;
                }
                newRoot = withY(newRoot, newRoot.y + (pointingUp_ ? -1 : 1));
            }
            radius_ /= 2;
        }
        return false;
    }

    void place(LocalModificationRegion& region, random::RandomSource& random, const Wind& wind) const {
        const std::int32_t dripstoneBlock = stateIdOf("dripstone_block");
        for (std::int32_t dx = -radius_; dx <= radius_; ++dx) {
            for (std::int32_t dz = -radius_; dz <= radius_; ++dz) {
                const auto currentRadius = std::sqrt(static_cast<float>(dx * dx + dz * dz));
                if (currentRadius > static_cast<float>(radius_)) {
                    continue;
                }
                std::int32_t height = heightAtRadius(currentRadius);
                if (height <= 0) {
                    continue;
                }
                if (random.nextFloat() < 0.2F) {
                    height = wire::common::toIntLikeJava(
                        static_cast<double>(static_cast<float>(height) * uniformFloat(random, 0.8F, 1.0F)));
                }
                DecorationPos position = offset(root_, dx, 0, dz);
                bool hasBeenOutOfStone = false;
                const std::int32_t maxY = pointingUp_ ? region.worldSurfaceHeight(position.x, position.z) : kNoCeiling;
                for (std::int32_t step = 0; step < height; ++step) {
                    if (position.y >= maxY) {
                        break;
                    }
                    const DecorationPos adjusted = wind.offsetOf(position);
                    if (region.isEmptyOrWaterOrLava(adjusted)) {
                        hasBeenOutOfStone = true;
                        region.set(adjusted, dripstoneBlock);
                    } else if (hasBeenOutOfStone && baseStoneOverworld().contains(region.blockPath(adjusted))) {
                        break;
                    }
                    position = withY(position, position.y + (pointingUp_ ? 1 : -1));
                }
            }
        }
    }

private:
    static constexpr std::int32_t kNoCeiling = 2147483647;

    [[nodiscard]] std::int32_t height() const { return heightAtRadius(0.0F); }

    [[nodiscard]] std::int32_t heightAtRadius(float checkRadius) const {
        return wire::common::toIntLikeJava(
            speleothemHeight(static_cast<double>(checkRadius), static_cast<double>(radius_), scale_, bluntness_));
    }

    DecorationPos root_;
    bool pointingUp_;
    std::int32_t radius_;
    double bluntness_;
    double scale_;
};

struct CaveColumn {
    std::int32_t floor;
    std::int32_t ceiling;

    [[nodiscard]] std::int32_t height() const { return ceiling - floor - 1; }
};

void addCrackPoints(std::vector<DecorationPos>& points, const DecorationPos& origin, std::int32_t xOffset,
                    std::int32_t zOffset) {
    points.push_back(offset(origin, xOffset, 7, zOffset));
    points.push_back(offset(origin, xOffset, 5, zOffset));
    points.push_back(offset(origin, xOffset, 1, zOffset));
}

void scheduleAdjacentFluids(LocalModificationRegion& region, const DecorationPos& position) {
    const std::int32_t lava = registry::blocks::lava().stateId;
    for (const Direction& direction : kDirections) {
        const DecorationPos adjacent = offset(position, direction);
        const std::optional<std::int32_t> stateId = region.get(adjacent);
        if (!stateId.has_value()) {
            continue;
        }
        if (registry::blockFluids::hasWaterFluid(*stateId)) {
            region.scheduleFluidTick(adjacent, wire::common::Identifier::vanilla("water"));
        } else if (*stateId >= lava && *stateId <= lava + 15) {
            region.scheduleFluidTick(adjacent, wire::common::Identifier::vanilla("lava"));
        }
    }
}

struct GeodePoint {
    DecorationPos position;
    std::int32_t offset;
};

bool placeGeode(LocalModificationRegion& region, random::RandomSource& random, std::int64_t seed,
                const DecorationPos& origin) {
    const std::int32_t amethystBlock = stateIdOf("amethyst_block");
    const std::int32_t buddingAmethyst = stateIdOf("budding_amethyst");
    const std::int32_t calcite = stateIdOf("calcite");
    const std::int32_t smoothBasalt = stateIdOf("smooth_basalt");

    std::vector<GeodePoint> points;
    const std::int32_t numPoints = 3 + random.nextInt(2);
    random::RandomSource noiseSource = random::RandomSource::worldgen(random::RandomSource::legacy(seed));
    const std::vector<double> amplitudes{1.0};
    const noise::NormalNoise noise = noise::NormalNoise::create(noiseSource, -4, amplitudes);
    std::vector<DecorationPos> crackPoints;
    const double crackSizeAdjustment = static_cast<double>(numPoints) / 6.0;
    const double innerAir = 1.0 / std::sqrt(1.7);
    const double innermostBlockLayer = 1.0 / std::sqrt(2.2 + crackSizeAdjustment);
    const double innerCrust = 1.0 / std::sqrt(3.2 + crackSizeAdjustment);
    const double outerCrust = 1.0 / std::sqrt(4.2 + crackSizeAdjustment);
    const double crackSize =
        1.0 / std::sqrt(2.0 + random.nextDouble() / 2.0 + (numPoints > 3 ? crackSizeAdjustment : 0.0));
    const bool shouldGenerateCrack = random.nextFloat() < 0.95F;
    std::int32_t invalidPoints = 0;

    for (std::int32_t index = 0; index < numPoints; ++index) {
        const DecorationPos point = offset(origin, 4 + random.nextInt(3), 4 + random.nextInt(3), 4 + random.nextInt(3));
        if (region.isAir(point) || geodeInvalidBlocks().contains(region.blockPath(point))) {
            ++invalidPoints;
            if (invalidPoints > 1) {
                return false;
            }
        }
        points.push_back(GeodePoint{.position = point, .offset = 1 + random.nextInt(2)});
    }

    if (shouldGenerateCrack) {
        const std::int32_t crackOffset = numPoints * 2 + 1;
        switch (random.nextInt(4)) {
        case 0:
            addCrackPoints(crackPoints, origin, crackOffset, 0);
            break;
        case 1:
            addCrackPoints(crackPoints, origin, 0, crackOffset);
            break;
        case 2:
            addCrackPoints(crackPoints, origin, crackOffset, crackOffset);
            break;
        default:
            addCrackPoints(crackPoints, origin, 0, 0);
            break;
        }
    }

    std::vector<DecorationPos> potentialCrystals;
    for (std::int32_t zOffset = -16; zOffset <= 16; ++zOffset) {
        for (std::int32_t yOffset = -16; yOffset <= 16; ++yOffset) {
            for (std::int32_t xOffset = -16; xOffset <= 16; ++xOffset) {
                const DecorationPos position = offset(origin, xOffset, yOffset, zOffset);
                const double noiseOffset =
                    noise.getValue(static_cast<double>(position.x), static_cast<double>(position.y),
                                   static_cast<double>(position.z)) *
                    0.05;
                double shellDistance = 0.0;
                double crackDistance = 0.0;
                for (const GeodePoint& point : points) {
                    shellDistance +=
                        inverseSqrt(distanceSquared(position, point.position) + point.offset) + noiseOffset;
                }
                for (const DecorationPos& point : crackPoints) {
                    crackDistance += inverseSqrt(distanceSquared(position, point) + 2) + noiseOffset;
                }
                if (shellDistance < outerCrust) {
                    continue;
                }
                if (shouldGenerateCrack && crackDistance >= crackSize && shellDistance < innerAir) {
                    region.safeSet(position, registry::blocks::air().stateId);
                    scheduleAdjacentFluids(region, position);
                } else if (shellDistance >= innerAir) {
                    region.safeSet(position, registry::blocks::air().stateId);
                } else if (shellDistance >= innermostBlockLayer) {
                    const bool alternate = random.nextFloat() < 0.083F;
                    region.safeSet(position, alternate ? buddingAmethyst : amethystBlock);
                    if (alternate && random.nextFloat() < 0.35F) {
                        potentialCrystals.push_back(position);
                    }
                } else if (shellDistance >= innerCrust) {
                    region.safeSet(position, calcite);
                } else {
                    region.safeSet(position, smoothBasalt);
                }
            }
        }
    }

    for (const DecorationPos& crystalPosition : potentialCrystals) {
        const std::string_view block = kAmethystPlacements[static_cast<std::size_t>(
            random.nextInt(static_cast<std::int32_t>(kAmethystPlacements.size())))];
        for (const Direction& direction : kDirections) {
            const DecorationPos placePosition = offset(crystalPosition, direction);
            const std::optional<std::int32_t> placeState = region.get(placePosition);
            if (!placeState.has_value() ||
                !caveLocalModificationDecoration::canAmethystClusterGrowAtState(*placeState)) {
                continue;
            }
            region.safeSet(placePosition,
                           amethystPlacementState(block, direction.name,
                                                  caveLocalModificationDecoration::isSourceAmethystWater(*placeState)));
            break;
        }
    }
    return true;
}

[[nodiscard]] std::optional<CaveColumn> scanColumn(const LocalModificationRegion& region, const DecorationPos& origin,
                                                   std::int32_t range) {
    const auto scan = [&](std::int32_t direction) -> std::optional<std::int32_t> {
        DecorationPos position = origin;
        for (std::int32_t step = 1; step < range; ++step) {
            if (!region.isEmptyOrWater(position)) {
                break;
            }
            position = withY(position, position.y + direction);
        }
        if (region.isDripstoneEdge(position)) {
            return position.y;
        }
        return std::nullopt;
    };
    const std::optional<std::int32_t> ceiling = scan(1);
    if (!ceiling.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> floor = scan(-1);
    if (!floor.has_value()) {
        return std::nullopt;
    }
    return CaveColumn{.floor = *floor, .ceiling = *ceiling};
}

bool placeLargeDripstone(LocalModificationRegion& region, random::RandomSource& random, const DecorationPos& origin) {
    if (!region.isEmptyOrWater(origin)) {
        return false;
    }
    const std::optional<CaveColumn> column = scanColumn(region, origin, 30);
    if (!column.has_value() || column->height() < 4) {
        return false;
    }

    const std::int32_t radiusLimit = std::clamp(
        wire::common::toIntLikeJava(static_cast<double>(static_cast<float>(column->height()) * 0.33F)), 3, 16);
    const std::int32_t radius = 3 + random.nextInt(radiusLimit - 3 + 1);
    LargeDripstone stalactite{withY(origin, column->ceiling - 1), false, radius,
                              static_cast<double>(uniformFloat(random, 0.3F, 0.9F)),
                              static_cast<double>(uniformFloat(random, 0.4F, 2.0F))};
    LargeDripstone stalagmite{withY(origin, column->floor + 1), true, radius,
                              static_cast<double>(uniformFloat(random, 0.4F, 1.0F)),
                              static_cast<double>(uniformFloat(random, 0.4F, 2.0F))};
    Wind wind;
    if (stalactite.isSuitableForWind() && stalagmite.isSuitableForWind()) {
        const float speed = uniformFloat(random, 0.0F, 0.3F);
        const float direction = uniformFloat(random, 0.0F, static_cast<float>(std::numbers::pi));
        wind = Wind{.active = true,
                    .originY = origin.y,
                    .speedX = wire::common::mth::cos(direction) * speed,
                    .speedZ = wire::common::mth::sin(direction) * speed,
                    .maxOffset = 16 - radius};
    }

    if (stalactite.embedBase(region, wind)) {
        stalactite.place(region, random, wind);
    }
    if (stalagmite.embedBase(region, wind)) {
        stalagmite.place(region, random, wind);
    }
    return true;
}

void placeAmethystGeode(LocalModificationRegion& region, random::RandomSource& random, std::int64_t seed,
                        const surface::SurfaceRuleEnvironment& environment) {
    if (random.nextFloat() >= 1.0F / 24.0F) {
        return;
    }
    const std::int32_t minY = region.chunk().verticalBounds().minY();
    const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
    const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
    const DecorationPos origin{.x = x, .y = minY + 6 + random.nextInt(30 - (minY + 6) + 1), .z = z};
    if (!overworldBiomePaths().contains(biomePath(environment.biomeAt(origin.x, origin.y, origin.z)))) {
        return;
    }
    static_cast<void>(placeGeode(region, random, seed, origin));
}

void placeLargeDripstones(LocalModificationRegion& region, random::RandomSource& random,
                          const surface::SurfaceRuleEnvironment& environment) {
    const std::int32_t minY = region.chunk().verticalBounds().minY();
    const std::int32_t count = 10 + random.nextInt(39);
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const DecorationPos origin{.x = x, .y = minY + random.nextInt(256 - minY + 1), .z = z};
        if (biomePath(environment.biomeAt(origin.x, origin.y, origin.z)) == "dripstone_caves") {
            static_cast<void>(placeLargeDripstone(region, random, origin));
        }
    }
}

} // namespace

bool caveLocalModificationDecoration::canAmethystClusterGrowAtState(const std::int32_t stateId) {
    if (registry::blocks::isAir(stateId)) {
        return true;
    }
    const std::optional<std::int32_t> level = amethystWaterLevel(stateId);
    return level.has_value() && (*level == 0 || *level == 8);
}

bool caveLocalModificationDecoration::isSourceAmethystWater(const std::int32_t stateId) {
    const std::optional<std::int32_t> level = amethystWaterLevel(stateId);
    return level.has_value() && *level == kSourceWaterLevel;
}

const std::set<std::string, std::less<>>& caveLocalModificationDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"amethyst_geode", "large_dripstone"};
    return ids;
}

void caveLocalModificationDecoration::decorateFeatures(Chunk& chunk, const std::int64_t seed,
                                                       const surface::SurfaceRuleEnvironment& environment,
                                                       const std::int32_t firstIndex, const std::int32_t lastIndex,
                                                       ExternalSurfaceHeightReader externalSurfaceHeight,
                                                       ExternalStateReader externalBlockState) {
    LocalModificationRegion region{chunk, std::move(externalSurfaceHeight), std::move(externalBlockState)};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (std::int32_t globalIndex = firstIndex; globalIndex <= lastIndex; ++globalIndex) {
        random::setFeatureSeed(random, decorationSeed, globalIndex, kLocalModificationsStep);
        if (globalIndex == kAmethystGeodeIndex) {
            placeAmethystGeode(region, random, seed, environment);
        } else if (globalIndex == kLargeDripstoneIndex) {
            placeLargeDripstones(region, random, environment);
        }
    }
}

void caveLocalModificationDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                               const surface::SurfaceRuleEnvironment& environment,
                                               ExternalSurfaceHeightReader externalSurfaceHeight,
                                               ExternalStateReader externalBlockState) {
    decorateFeatures(chunk, seed, environment, kAmethystGeodeIndex, kLargeDripstoneIndex,
                     std::move(externalSurfaceHeight), std::move(externalBlockState));
}

bool caveLocalModificationDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                              const std::string_view placedFeature) {
    LocalModificationRegion region{chunk, nullptr, nullptr};
    if (placedFeature == "amethyst_geode") {
        static_cast<void>(placeGeode(region, random, chunk.chunkX(), origin));
        return true;
    }
    if (placedFeature == "large_dripstone") {
        static_cast<void>(placeLargeDripstone(region, random, origin));
        return true;
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/RootedSulfurSpringFeature.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

using structure::StructurePos;
using structure::StructureRotation;
using structure::StructureTemplate;

constexpr std::int32_t kRootColumnMaxHeight = 184;
constexpr std::int32_t kRequiredVerticalSpace = 5;
constexpr std::int32_t kAllowedVerticalWater = 1;
constexpr std::int32_t kLevelTestDistance = 8;
constexpr std::int32_t kMaxLevelDeviation = 2;
constexpr std::int32_t kRootPlacementAttempts = 20;
constexpr std::int32_t kRootRadius = 3;
constexpr std::int32_t kHangingRootPlacementAttempts = 1;
constexpr std::int32_t kHangingRootRadius = 1;
constexpr std::int32_t kHangingRootVerticalSpan = 1;
constexpr std::int32_t kEnvironmentScanSteps = 4;

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kLevelTestDirections{
    std::pair{0, -kLevelTestDistance}, std::pair{kLevelTestDistance, 0}, std::pair{0, kLevelTestDistance},
    std::pair{-kLevelTestDistance, 0}};

constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};

struct Variant {
    std::int32_t weight;
    std::int32_t tuffAttempts;
    std::int32_t radius;
    std::vector<wire::common::Identifier> templates;
};

[[nodiscard]] std::vector<wire::common::Identifier> templateNames(std::string_view size, std::int32_t count) {
    std::vector<wire::common::Identifier> names;
    names.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 1; index <= count; ++index) {
        names.push_back(wire::common::Identifier::vanilla("spring/sulfur_spring_" + std::string{size} + "_" +
                                                          std::to_string(index)));
    }
    return names;
}

const std::vector<Variant>& variants() {
    static const std::vector<Variant> built = {
        Variant{.weight = 200, .tuffAttempts = 64, .radius = 7, .templates = templateNames("small", 4)},
        Variant{.weight = 90, .tuffAttempts = 80, .radius = 8, .templates = templateNames("medium", 3)},
        Variant{.weight = 20, .tuffAttempts = 96, .radius = 9, .templates = templateNames("large", 2)},
        Variant{.weight = 5, .tuffAttempts = 128, .radius = 10, .templates = templateNames("extra_large", 1)}};
    return built;
}

[[nodiscard]] std::int32_t totalVariantWeight() {
    static const std::int32_t total = [] {
        std::int32_t sum = 0;
        for (const Variant& variant : variants()) {
            sum += variant.weight;
        }
        return sum;
    }();
    return total;
}

const std::set<std::string, std::less<>>& rootReplaceable() {
    static const std::set<std::string, std::less<>> paths = {"stone",
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
                                                             "red_sand",
                                                             "clay",
                                                             "gravel",
                                                             "sand",
                                                             "snow_block",
                                                             "powder_snow"};
    return paths;
}

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t tuffStateId() {
    static const std::int32_t stateId = state("tuff");
    return stateId;
}

[[nodiscard]] std::int32_t sulfurStateId() {
    static const std::int32_t stateId = state("sulfur");
    return stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] DecorationPos offsetBy(const DecorationPos& position, std::int32_t dx, std::int32_t dy, std::int32_t dz) {
    return DecorationPos{.x = position.x + dx, .y = position.y + dy, .z = position.z + dz};
}

[[nodiscard]] DecorationPos withY(const DecorationPos& position, std::int32_t y) {
    return DecorationPos{.x = position.x, .y = y, .z = position.z};
}

class Region {
public:
    Region(Chunk& chunk, const rootedSulfurSpringFeature::BlockStateAt& blockAt) : chunk_(&chunk), blockAt_(&blockAt) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    [[nodiscard]] std::int32_t get(const DecorationPos& position) const { return (*blockAt_)(position); }

    void set(const DecorationPos& position, std::int32_t stateId) const {
        const std::int32_t expected = get(position);
        chunk_->setDecorationBlockStateId(position.x, position.y, position.z, stateId, expected);
    }

private:
    Chunk* chunk_;
    const rootedSulfurSpringFeature::BlockStateAt* blockAt_;
};

/// parity: `TrapezoidInt(-radius, radius, 0)`'s optimised symmetric branch.
[[nodiscard]] std::int32_t triangular(random::RandomSource& random, std::int32_t radius) {
    return random.nextInt(radius + 1) - random.nextInt(radius + 1);
}

/// parity: an environment scan downward with four steps, including the start and the fourth.
[[nodiscard]] std::optional<DecorationPos> scanDownForSolid(const Region& region, const DecorationPos& origin) {
    DecorationPos position = origin;
    for (std::int32_t step = 0; step <= kEnvironmentScanSteps; ++step) {
        if (registry::blockShapes::isSolidState(region.get(position))) {
            return position;
        }
        if (step != kEnvironmentScanSteps) {
            position = offsetBy(position, 0, -1, 0);
        }
    }
    return std::nullopt;
}

[[nodiscard]] const Variant& weightedVariant(random::RandomSource& random) {
    std::int32_t roll = random.nextInt(totalVariantWeight());
    for (const Variant& variant : variants()) {
        if (roll < variant.weight) {
            return variant;
        }
        roll -= variant.weight;
    }
    return variants().back();
}

[[nodiscard]] bool spaceForSpring(const Region& region, const DecorationPos& candidate) {
    for (std::int32_t distance = 1; distance <= kRequiredVerticalSpace; ++distance) {
        const std::int32_t stateId = region.get(offsetBy(candidate, 0, distance, 0));
        const bool water = distance + 1 <= kAllowedVerticalWater && registry::blockFluids::hasWaterFluid(stateId);
        if (!registry::blocks::isAir(stateId) && !water) {
            return false;
        }
    }
    for (const auto& [dx, dz] : kLevelTestDirections) {
        const DecorationPos probe = offsetBy(candidate, dx, 0, dz);
        if (registry::blocks::isAir(region.get(offsetBy(probe, 0, -kMaxLevelDeviation, 0)))) {
            return false;
        }
        if (!registry::blocks::isAir(region.get(offsetBy(probe, 0, kMaxLevelDeviation, 0)))) {
            return false;
        }
    }
    return true;
}

bool placeSulfurSpring(const Region& region, random::RandomSource& random, const DecorationPos& origin,
                       const structure::TemplateLoader& templates) {
    const Variant& variant = weightedVariant(random);
    bool placedTuff = false;
    for (std::int32_t attempt = 0; attempt < variant.tuffAttempts; ++attempt) {
        const DecorationPos position = offsetBy(origin, triangular(random, variant.radius), triangular(random, 3),
                                                triangular(random, variant.radius));
        const std::optional<DecorationPos> ground = scanDownForSolid(region, position);
        if (!ground.has_value()) {
            continue;
        }
        region.set(*ground, tuffStateId());
        placedTuff = true;
    }
    if (!placedTuff) {
        return false;
    }

    // A weighted list draws even for its one-entry extra-large case.
    const wire::common::Identifier& templateId =
        variant
            .templates[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(variant.templates.size())))];
    const StructureTemplate* found = templates(templateId);
    if (found == nullptr) {
        return false;
    }
    const StructureRotation rotation =
        kRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kRotations.size())))];
    const std::int32_t halfX = found->size().x / 2;
    const std::int32_t halfZ = found->size().z / 2;
    StructurePos offset{};
    switch (rotation) {
    case StructureRotation::Clockwise90:
        offset = StructurePos{halfZ, 0, -halfX};
        break;
    case StructureRotation::Clockwise180:
        offset = StructurePos{halfX, 0, halfZ};
        break;
    case StructureRotation::Counterclockwise90:
        offset = StructurePos{-halfZ, 0, halfX};
        break;
    case StructureRotation::None:
    default:
        offset = StructurePos{-halfX, 0, -halfZ};
        break;
    }
    return found->placeDecorationInto(
        region.chunk(), StructurePos{origin.x + offset.x, origin.y - 7, origin.z + offset.z}, rotation, random);
}

void placeSulfurRoots(const Region& region, random::RandomSource& random, const DecorationPos& origin,
                      std::int32_t targetHeight) {
    for (std::int32_t y = origin.y; y < targetHeight; ++y) {
        for (std::int32_t attempt = 0; attempt < kRootPlacementAttempts; ++attempt) {
            const DecorationPos position{origin.x + random.nextInt(kRootRadius) - random.nextInt(kRootRadius), y,
                                         origin.z + random.nextInt(kRootRadius) - random.nextInt(kRootRadius)};
            if (rootReplaceable().contains(pathOf(region.get(position)))) {
                region.set(position, sulfurStateId());
            }
        }
    }
}

void placeHangingSulfur(const Region& region, random::RandomSource& random, const DecorationPos& origin) {
    for (std::int32_t attempt = 0; attempt < kHangingRootPlacementAttempts; ++attempt) {
        // Every offset provider is zero-width, but all six draws still happen.
        const DecorationPos position{
            origin.x + random.nextInt(kHangingRootRadius) - random.nextInt(kHangingRootRadius),
            origin.y + random.nextInt(kHangingRootVerticalSpan) - random.nextInt(kHangingRootVerticalSpan),
            origin.z + random.nextInt(kHangingRootRadius) - random.nextInt(kHangingRootRadius)};
        if (!registry::blocks::isAir(region.get(position))) {
            continue;
        }
        const std::int32_t above = region.get(offsetBy(position, 0, 1, 0));
        if (registry::blockShapes::isFaceSturdy(above, registry::BlockFace::Down)) {
            region.set(position, sulfurStateId());
        }
    }
}

} // namespace

bool rootedSulfurSpringFeature::place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                      const structure::TemplateLoader& templates, const BlockStateAt& blockStateAt) {
    const Region region{chunk, blockStateAt};
    if (!registry::blocks::isAir(region.get(origin))) {
        return false;
    }

    DecorationPos candidate = origin;
    for (std::int32_t columnOffset = 0; columnOffset < kRootColumnMaxHeight; ++columnOffset) {
        candidate = withY(candidate, candidate.y + 1);
        if (chunk.heightAt(candidate.x & 15, candidate.z & 15, HeightmapType::WorldSurface) < candidate.y) {
            return true;
        }
        if (!registry::blocks::isAir(region.get(candidate)) || !spaceForSpring(region, candidate)) {
            continue;
        }
        const std::int32_t below = region.get(offsetBy(candidate, 0, -1, 0));
        if (pathOf(below) == "lava" || !registry::blockShapes::isSolidState(below)) {
            return true;
        }
        if (!placeSulfurSpring(region, random, candidate, templates)) {
            continue;
        }
        placeSulfurRoots(region, random, origin, origin.y + columnOffset);
        placeHangingSulfur(region, random, origin);
        return true;
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

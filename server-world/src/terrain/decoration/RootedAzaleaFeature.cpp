#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/RootedAzaleaFeature.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kRootColumnMaxHeight = 100;
constexpr std::int32_t kRequiredVerticalSpace = 3;
constexpr std::int32_t kAllowedVerticalWater = 2;
constexpr std::int32_t kRootPlacementAttempts = 20;
constexpr std::int32_t kRootRadius = 3;
constexpr std::int32_t kHangingRootPlacementAttempts = 20;
constexpr std::int32_t kHangingRootRadius = 3;
constexpr std::int32_t kHangingRootVerticalSpan = 2;

using PathSet = std::set<std::string, std::less<>>;

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

const PathSet& treeReplaceable() {
    static const PathSet paths = {"oak_leaves",
                                  "spruce_leaves",
                                  "birch_leaves",
                                  "jungle_leaves",
                                  "acacia_leaves",
                                  "dark_oak_leaves",
                                  "mangrove_leaves",
                                  "cherry_leaves",
                                  "pale_oak_leaves",
                                  "azalea_leaves",
                                  "flowering_azalea_leaves",
                                  "dandelion",
                                  "open_eyeblossom",
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
                                  "wither_rose",
                                  "torchflower",
                                  "closed_eyeblossom",
                                  "golden_dandelion",
                                  "short_grass",
                                  "fern",
                                  "dead_bush",
                                  "vine",
                                  "glow_lichen",
                                  "sunflower",
                                  "lilac",
                                  "rose_bush",
                                  "peony",
                                  "hanging_roots",
                                  "tall_grass",
                                  "large_fern",
                                  "pale_moss_carpet",
                                  "pitcher_plant",
                                  "water",
                                  "seagrass",
                                  "tall_seagrass",
                                  "bush",
                                  "firefly_bush",
                                  "warped_roots",
                                  "nether_sprouts",
                                  "crimson_roots",
                                  "leaf_litter",
                                  "short_dry_grass",
                                  "tall_dry_grass"};
    return paths;
}

const PathSet& azaleaGrowsOn() {
    static const PathSet paths = {"stone",
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
                                  "sand",
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
                                  "black_terracotta",
                                  "snow_block",
                                  "powder_snow"};
    return paths;
}

const PathSet& rootReplaceable() {
    static const PathSet paths = [] {
        PathSet built = azaleaGrowsOn();
        built.erase("suspicious_sand");
        for (const std::string_view path : {"granite", "diorite", "andesite", "tuff", "deepslate", "clay", "gravel"}) {
            built.emplace(path);
        }
        return built;
    }();
    return paths;
}

[[nodiscard]] std::int32_t blockAt(const Chunk& chunk, const DecorationPos& position) {
    return chunk.decorationBlockStateId(position.x, position.y, position.z);
}

void set(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId);
}

[[nodiscard]] bool isTreeReplaceable(std::int32_t stateId) {
    return registry::blocks::isAir(stateId) || treeReplaceable().contains(pathOf(stateId));
}

[[nodiscard]] bool aboveSearchLimit(const Chunk& chunk, const DecorationPos& position) {
    // heightAt, not the whole heightmap: this runs once per step of a hundred-step climb.
    return position.y > chunk.verticalBounds().maxY() ||
           position.y > chunk.heightAt(position.x & 15, position.z & 15, HeightmapType::WorldSurface);
}

[[nodiscard]] bool allowedTreePosition(const Chunk& chunk, const DecorationPos& position) {
    const std::int32_t below = blockAt(chunk, DecorationPos{.x = position.x, .y = position.y - 1, .z = position.z});
    return isTreeReplaceable(blockAt(chunk, position)) && azaleaGrowsOn().contains(pathOf(below));
}

[[nodiscard]] bool spaceForTree(const Chunk& chunk, const DecorationPos& position) {
    for (std::int32_t distance = 1; distance <= kRequiredVerticalSpace; ++distance) {
        const std::int32_t stateId =
            blockAt(chunk, DecorationPos{.x = position.x, .y = position.y + distance, .z = position.z});
        if (registry::blocks::isAir(stateId)) {
            continue;
        }
        if (distance + 1 <= kAllowedVerticalWater && registry::blockFluids::hasWaterFluid(stateId)) {
            continue;
        }
        return false;
    }
    return true;
}

void placeRootedDirt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                     std::int32_t targetHeight) {
    for (std::int32_t y = origin.y; y < targetHeight; ++y) {
        for (std::int32_t attempt = 0; attempt < kRootPlacementAttempts; ++attempt) {
            const DecorationPos position{.x = origin.x + random.nextInt(kRootRadius) - random.nextInt(kRootRadius),
                                         .y = y,
                                         .z = origin.z + random.nextInt(kRootRadius) - random.nextInt(kRootRadius)};
            if (rootReplaceable().contains(pathOf(blockAt(chunk, position)))) {
                set(chunk, position, state("rooted_dirt"));
            }
        }
    }
}

void placeHangingRoots(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    for (std::int32_t attempt = 0; attempt < kHangingRootPlacementAttempts; ++attempt) {
        const DecorationPos position{
            .x = origin.x + random.nextInt(kHangingRootRadius) - random.nextInt(kHangingRootRadius),
            .y = origin.y + random.nextInt(kHangingRootVerticalSpan) - random.nextInt(kHangingRootVerticalSpan),
            .z = origin.z + random.nextInt(kHangingRootRadius) - random.nextInt(kHangingRootRadius)};
        if (!registry::blocks::isAir(blockAt(chunk, position))) {
            continue;
        }
        const std::int32_t above = blockAt(chunk, DecorationPos{.x = position.x, .y = position.y + 1, .z = position.z});
        if (registry::blockShapes::isFaceSturdy(above, registry::BlockFace::Down)) {
            set(chunk, position, state("hanging_roots"));
        }
    }
}

} // namespace

bool rootedAzaleaFeature::place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    if (!registry::blocks::isAir(blockAt(chunk, origin))) {
        return false;
    }
    DecorationPos position = origin;
    for (std::int32_t columnOffset = 0; columnOffset < kRootColumnMaxHeight; ++columnOffset) {
        position.y++;
        if (aboveSearchLimit(chunk, position)) {
            return true;
        }
        if (!allowedTreePosition(chunk, position) || !spaceForTree(chunk, position)) {
            continue;
        }
        const std::int32_t below = blockAt(chunk, DecorationPos{.x = position.x, .y = position.y - 1, .z = position.z});
        // parity: the test is `BlockState.isSolid`, not a sturdy top face. A fence passes the first
        // and fails the second, so the two disagree on where an azalea may root.
        if (pathOf(below) == "lava" || !registry::blockShapes::isSolidState(below)) {
            return true;
        }
        if (!tree::treeFeature::place(chunk, random, position.x, position.y, position.z, tree::treeConfigs::azalea())) {
            continue;
        }
        placeRootedDirt(chunk, random, origin, origin.y + columnOffset);
        placeHangingRoots(chunk, random, origin);
        return true;
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

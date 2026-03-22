#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/HugeMushroomFeature.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

using PathSet = std::set<std::string, std::less<>>;

[[nodiscard]] std::int32_t state(std::string_view name) {
    const registry::BlockState* found = registry::blocks::named(name);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{name} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::int32_t mushroomState(std::int32_t stateId, bool down, bool east, bool north, bool south, bool up,
                                         bool west) {
    const std::optional<std::int32_t> resolved =
        registry::blockStateFamilies::hugeMushroomStateId(stateId, down, east, north, south, up, west);
    if (!resolved.has_value()) {
        throw std::runtime_error{"invalid huge mushroom state"};
    }
    return *resolved;
}

[[nodiscard]] std::int32_t stemStateId() {
    static const std::int32_t stateId = mushroomState(state("mushroom_stem"), false, true, true, true, false, true);
    return stateId;
}

// parity: block tags huge_brown/red_mushroom_can_place_on, whose contents are identical.
const PathSet& canPlaceOnPaths() {
    static const PathSet paths = {"dirt",           "coarse_dirt",     "rooted_dirt", "mud",    "muddy_mangrove_roots",
                                  "moss_block",     "pale_moss_block", "grass_block", "podzol", "mycelium",
                                  "crimson_nylium", "warped_nylium"};
    return paths;
}

// parity: block tag replaceable_by_mushrooms.
const PathSet& replaceablePaths() {
    static const PathSet paths = {"jungle_leaves",
                                  "oak_leaves",
                                  "spruce_leaves",
                                  "pale_oak_leaves",
                                  "dark_oak_leaves",
                                  "acacia_leaves",
                                  "birch_leaves",
                                  "azalea_leaves",
                                  "flowering_azalea_leaves",
                                  "mangrove_leaves",
                                  "cherry_leaves",
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
                                  "pale_moss_carpet",
                                  "short_grass",
                                  "fern",
                                  "dead_bush",
                                  "vine",
                                  "glow_lichen",
                                  "sunflower",
                                  "lilac",
                                  "rose_bush",
                                  "peony",
                                  "tall_grass",
                                  "large_fern",
                                  "hanging_roots",
                                  "pitcher_plant",
                                  "water",
                                  "seagrass",
                                  "tall_seagrass",
                                  "brown_mushroom",
                                  "red_mushroom",
                                  "brown_mushroom_block",
                                  "red_mushroom_block",
                                  "warped_roots",
                                  "nether_sprouts",
                                  "crimson_roots",
                                  "leaf_litter",
                                  "short_dry_grass",
                                  "tall_dry_grass",
                                  "bush",
                                  "firefly_bush",
                                  "orange_poplar_leaves",
                                  "red_poplar_leaves",
                                  "yellow_poplar_leaves",
                                  "red_shrub"};
    return paths;
}

/// parity: `AbstractHugeMushroomFeature.placeMushroomBlock`.
void placeMushroomBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    const std::int32_t current = chunk.decorationBlockStateId(x, y, z);
    if (registry::blocks::isAir(current) || replaceablePaths().contains(pathOf(current))) {
        chunk.setDecorationBlockStateId(x, y, z, stateId);
    }
}

[[nodiscard]] bool isValidPosition(const Chunk& chunk, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
                                   std::int32_t treeHeight, hugeMushroomFeature::Kind kind) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (originY < bounds.minY() + 1 || originY + treeHeight + 1 > bounds.maxY()) {
        return false;
    }
    if (!canPlaceOnPaths().contains(pathOf(chunk.decorationBlockStateId(originX, originY - 1, originZ)))) {
        return false;
    }
    // parity: the real isValidPosition passes treeHeight = -1 into getTreeRadiusForHeight, so the red
    // variant's own radius rule resolves to 0 for every checked layer and brown's to `dy <= 3 ? 0 : 3`.
    // The cap radius therefore only matters for brown here.
    for (std::int32_t dy = 0; dy <= treeHeight; ++dy) {
        const std::int32_t radius = kind == hugeMushroomFeature::Kind::Brown ? (dy <= 3 ? 0 : 3) : 0;
        for (std::int32_t dx = -radius; dx <= radius; ++dx) {
            for (std::int32_t dz = -radius; dz <= radius; ++dz) {
                const std::int32_t y = originY + dy;
                if (y > bounds.maxY()) {
                    return false;
                }
                const std::int32_t id = chunk.decorationBlockStateId(originX + dx, y, originZ + dz);
                if (!registry::blocks::isAir(id) && !registry::blockStateFamilies::isLeavesState(id)) {
                    return false;
                }
            }
        }
    }
    return true;
}

void makeCap(Chunk& chunk, std::int32_t originX, std::int32_t originY, std::int32_t originZ, std::int32_t treeHeight,
             hugeMushroomFeature::Kind kind) {
    if (kind == hugeMushroomFeature::Kind::Brown) {
        // parity: HugeBrownMushroomFeature.makeCap, radius 3, one layer at treeHeight, no corners.
        const std::int32_t capStateId = state("brown_mushroom_block");
        for (std::int32_t dx = -3; dx <= 3; ++dx) {
            for (std::int32_t dz = -3; dz <= 3; ++dz) {
                const bool xEdge = dx == -3 || dx == 3;
                const bool zEdge = dz == -3 || dz == 3;
                if (xEdge && zEdge) {
                    continue;
                }
                const bool west = dx == -3 || (zEdge && dx == -2);
                const bool east = dx == 3 || (zEdge && dx == 2);
                const bool north = dz == -3 || (xEdge && dz == -2);
                const bool south = dz == 3 || (xEdge && dz == 2);
                placeMushroomBlock(chunk, originX + dx, originY + treeHeight, originZ + dz,
                                   mushroomState(capStateId, false, east, north, south, true, west));
            }
        }
        return;
    }
    // parity: HugeRedMushroomFeature.makeCap, radius 2 over four layers, top layer radius 1.
    const std::int32_t capStateId = state("red_mushroom_block");
    for (std::int32_t dy = treeHeight - 3; dy <= treeHeight; ++dy) {
        const std::int32_t radius = dy < treeHeight ? 2 : 1;
        for (std::int32_t dx = -radius; dx <= radius; ++dx) {
            for (std::int32_t dz = -radius; dz <= radius; ++dz) {
                const bool xEdge = dx == -radius || dx == radius;
                const bool zEdge = dz == -radius || dz == radius;
                if (dy < treeHeight && xEdge == zEdge) {
                    continue;
                }
                placeMushroomBlock(
                    chunk, originX + dx, originY + dy, originZ + dz,
                    mushroomState(capStateId, false, dx > 0, dz < 0, dz > 0, dy >= treeHeight - 1, dx < 0));
            }
        }
    }
}

} // namespace

bool hugeMushroomFeature::place(Chunk& chunk, random::RandomSource& random, const std::int32_t originX,
                                const std::int32_t originY, const std::int32_t originZ, const Kind kind) {
    // parity: getTreeHeight, drawn before the validity check exactly like the real jar.
    std::int32_t treeHeight = random.nextInt(3) + 4;
    if (random.nextInt(12) == 0) {
        treeHeight *= 2;
    }
    if (!isValidPosition(chunk, originX, originY, originZ, treeHeight, kind)) {
        return false;
    }
    makeCap(chunk, originX, originY, originZ, treeHeight, kind);
    for (std::int32_t dy = 0; dy < treeHeight; ++dy) {
        placeMushroomBlock(chunk, originX, originY + dy, originZ, stemStateId());
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration

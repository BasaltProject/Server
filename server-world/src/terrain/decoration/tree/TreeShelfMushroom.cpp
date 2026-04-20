#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeatureDecorators.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

struct Facing {
    std::int32_t dx;
    std::int32_t dz;
    const char* name;
};

/// parity: `Direction.Plane.HORIZONTAL` order, each followed by its clockwise neighbour.
constexpr std::array<Facing, 4> kHorizontal{Facing{0, -1, "north"}, Facing{1, 0, "east"}, Facing{0, 1, "south"},
                                            Facing{-1, 0, "west"}};

constexpr std::int32_t kMinHeightOffset = 1;
constexpr std::int32_t kMaxHeightOffset = 4;
constexpr float kPerSideChance = 0.25F;

[[nodiscard]] std::int32_t stateAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

[[nodiscard]] bool isShelfMushroom(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateAt(chunk, x, y, z));
    return block != nullptr && block->path() == "shelf_mushroom";
}

[[nodiscard]] bool isWater(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateAt(chunk, x, y, z));
    return block != nullptr && block->path() == "water";
}

/// parity: `ShelfMushroomDecorator.isBlockReplaceableWithShelfMushroom`.
[[nodiscard]] bool canHold(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    return registry::blockShapes::isReplaceable(stateAt(chunk, x, y, z)) && !isWater(chunk, x, y, z) &&
           !isWater(chunk, x + 1, y, z) && !isWater(chunk, x - 1, y, z) && !isWater(chunk, x, y, z - 1) &&
           !isWater(chunk, x, y, z + 1);
}

[[nodiscard]] bool hasShelfMushroomBeside(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const Facing& facing : kHorizontal) {
        if (isShelfMushroom(chunk, x + facing.dx, y, z + facing.dz)) {
            return true;
        }
    }
    return false;
}

void placeMushroom(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                   const Facing& facing) {
    const std::int32_t age = random.nextInt(2);
    const std::optional<std::int32_t> state = registry::blocks::stateIdOf(
        wire::common::Identifier::vanilla("shelf_mushroom"), {{"age", std::to_string(age)}, {"facing", facing.name}});
    if (state.has_value()) {
        setBlock(chunk, x, y, z, *state);
    }
}

} // namespace

void treeDecorators::decorateShelfMushroom(Chunk& chunk, random::RandomSource& random,
                                           const std::vector<BlockPos>& logs, const float probability) {
    if (random.nextFloat() >= probability || logs.empty()) {
        return;
    }
    if (logs.front().y == logs.back().y) {
        // parity: a fallen log takes a mushroom on either long side, never next to another one.
        const bool alongX = logs.front().x != logs.back().x;
        const std::array<Facing, 2> sides = alongX ? std::array<Facing, 2>{kHorizontal[0], kHorizontal[2]}
                                                   : std::array<Facing, 2>{kHorizontal[1], kHorizontal[3]};
        for (const BlockPos& log : logs) {
            for (const Facing& side : sides) {
                if (random.nextFloat() > kPerSideChance) {
                    continue;
                }
                const std::int32_t x = log.x + side.dx;
                const std::int32_t z = log.z + side.dz;
                if (canHold(chunk, x, log.y, z) && !hasShelfMushroomBeside(chunk, x, log.y, z) &&
                    !hasShelfMushroomBeside(chunk, log.x, log.y, log.z)) {
                    placeMushroom(chunk, random, x, log.y, z, side);
                }
            }
        }
        return;
    }
    const std::size_t first = static_cast<std::size_t>(random.nextInt(4));
    const std::array<Facing, 2> sides{kHorizontal[first], kHorizontal[(first + 1) % 4]};
    const std::int32_t baseY = logs.front().y;
    for (const BlockPos& log : logs) {
        const std::int32_t dy = log.y - baseY;
        if (dy < kMinHeightOffset || dy > kMaxHeightOffset) {
            continue;
        }
        for (const Facing& side : sides) {
            if (random.nextFloat() > kPerSideChance) {
                continue;
            }
            const std::int32_t x = log.x + side.dx;
            const std::int32_t z = log.z + side.dz;
            if (!canHold(chunk, x, log.y, z) || isShelfMushroom(chunk, x, log.y - 1, z)) {
                continue;
            }
            placeMushroom(chunk, random, x, log.y, z, side);
            break;
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::tree

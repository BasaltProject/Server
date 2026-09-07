#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/PaleMossDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeatureDecorators.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

struct VinePlacement {
    std::int32_t dx = 0;
    std::int32_t dz = 0;
    std::int32_t faceBit = 0;
};

struct CocoaPlacement {
    std::int32_t dx = 0;
    std::int32_t dz = 0;
    std::int32_t facingOffset = 0;
};

constexpr std::array<VinePlacement, 4> kTrunkVines{
    VinePlacement{.dx = -1, .dz = 0, .faceBit = 16}, VinePlacement{.dx = 1, .dz = 0, .faceBit = 1},
    VinePlacement{.dx = 0, .dz = -1, .faceBit = 4}, VinePlacement{.dx = 0, .dz = 1, .faceBit = 8}};

constexpr std::array<CocoaPlacement, 4> kCocoaDirections{
    CocoaPlacement{.dx = 0, .dz = 1, .facingOffset = 0}, CocoaPlacement{.dx = -1, .dz = 0, .facingOffset = 3},
    CocoaPlacement{.dx = 0, .dz = -1, .facingOffset = 1}, CocoaPlacement{.dx = 1, .dz = 0, .facingOffset = 2}};

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 3> kBeehiveSpawnDirections{std::pair{1, 0}, std::pair{0, 1},
                                                                                       std::pair{-1, 0}};

constexpr std::array<std::int32_t, 4> kLeafLitterFacingStateOffsets{0, 12, 4, 8};

constexpr std::array<std::array<std::int32_t, 3>, 6> kBlockNeighbours{
    std::array<std::int32_t, 3>{0, -1, 0}, std::array<std::int32_t, 3>{0, 1, 0},  std::array<std::int32_t, 3>{0, 0, -1},
    std::array<std::int32_t, 3>{0, 0, 1},  std::array<std::int32_t, 3>{-1, 0, 0}, std::array<std::int32_t, 3>{1, 0, 0}};

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

const std::set<std::string, std::less<>>& beneathTreePodzolReplaceablePaths() {
    static const std::set<std::string, std::less<>> paths = {
        "dirt",       "coarse_dirt",     "rooted_dirt", "mud",    "muddy_mangrove_roots",
        "moss_block", "pale_moss_block", "grass_block", "podzol", "mycelium"};
    return paths;
}

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    return y >= bounds.minY() && y <= bounds.maxY() && registry::blocks::isAir(chunk.decorationBlockStateId(x, y, z));
}

void placePodzolAt(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t dy = 2; dy >= -3; --dy) {
        const std::int32_t cursorY = y + dy;
        if (cursorY < bounds.minY() || cursorY > bounds.maxY()) {
            continue;
        }
        const std::int32_t stateId = chunk.decorationBlockStateId(x, cursorY, z);
        if (beneathTreePodzolReplaceablePaths().contains(pathOf(stateId))) {
            chunk.setDecorationBlockStateId(x, cursorY, z, stateIdOf("podzol"));
            return;
        }
        if (!registry::blocks::isAir(stateId) && dy < 0) {
            return;
        }
    }
}

void placePodzolCircle(Chunk& chunk, std::int32_t centerX, std::int32_t centerY, std::int32_t centerZ) {
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            if (std::abs(dx) != 2 || std::abs(dz) != 2) {
                placePodzolAt(chunk, centerX + dx, centerY, centerZ + dz);
            }
        }
    }
}

void addHangingVine(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t faceBit) {
    const std::int32_t stateId = stateIdOf("vine") - faceBit;
    setBlock(chunk, x, y, z, stateId);
    std::int32_t cursorY = y - 1;
    for (std::int32_t remaining = 4; remaining > 0 && isAir(chunk, x, cursorY, z); --remaining) {
        setBlock(chunk, x, cursorY, z, stateId);
        cursorY--;
    }
}

void addPaleMossHanger(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t startY,
                       std::int32_t z) {
    const std::int32_t tipTrue = stateIdOf("pale_hanging_moss");
    std::int32_t y = startY;
    while (isAir(chunk, x, y - 1, z) && random.nextFloat() >= 0.5F) {
        setBlock(chunk, x, y, z, tipTrue + 1);
        y--;
    }
    setBlock(chunk, x, y, z, tipTrue);
}

/// The lowest layer of a tree's own support, which is its roots when they sit below the trunk.
[[nodiscard]] std::vector<BlockPos> lowestTrunkOrRootPositions(const TreeDecoratorContext& context) {
    if (context.roots.empty()) {
        return context.logs;
    }
    if (!context.logs.empty() && context.roots.front().y == context.logs.front().y) {
        std::vector<BlockPos> combined = context.logs;
        combined.insert(combined.end(), context.roots.begin(), context.roots.end());
        return combined;
    }
    return context.roots;
}

void alterGroundToPodzol(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& logs,
                         const std::vector<BlockPos>& roots) {
    const std::vector<BlockPos> blockPositions = lowestTrunkOrRootPositions({.logs = logs, .roots = roots});
    if (blockPositions.empty()) {
        return;
    }
    const std::int32_t minY = blockPositions.front().y;
    for (const BlockPos& log : blockPositions) {
        if (log.y != minY) {
            continue;
        }
        placePodzolCircle(chunk, log.x - 1, log.y, log.z - 1);
        placePodzolCircle(chunk, log.x + 2, log.y, log.z - 1);
        placePodzolCircle(chunk, log.x - 1, log.y, log.z + 2);
        placePodzolCircle(chunk, log.x + 2, log.y, log.z + 2);
        for (std::int32_t attempt = 0; attempt < 5; ++attempt) {
            const std::int32_t placement = random.nextInt(64);
            const std::int32_t x = placement % 8;
            const std::int32_t z = placement / 8;
            if (x == 0 || x == 7 || z == 0 || z == 7) {
                placePodzolCircle(chunk, log.x - 3 + x, log.y, log.z - 3 + z);
            }
        }
    }
}

void decorateTrunkVines(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& logs) {
    for (const BlockPos& log : logs) {
        for (const VinePlacement& vine : kTrunkVines) {
            if (random.nextInt(3) > 0 && isAir(chunk, log.x + vine.dx, log.y, log.z + vine.dz)) {
                setBlock(chunk, log.x + vine.dx, log.y, log.z + vine.dz, stateIdOf("vine") - vine.faceBit);
            }
        }
    }
}

void decorateLeafVines(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& leaves,
                       float probability) {
    for (const BlockPos& leaf : leaves) {
        for (const VinePlacement& vine : kTrunkVines) {
            if (random.nextFloat() < probability && isAir(chunk, leaf.x + vine.dx, leaf.y, leaf.z + vine.dz)) {
                addHangingVine(chunk, leaf.x + vine.dx, leaf.y, leaf.z + vine.dz, vine.faceBit);
            }
        }
    }
}

void decorateCocoa(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& logs, float probability) {
    if (random.nextFloat() >= probability || logs.empty()) {
        return;
    }
    const std::int32_t treeY = logs.front().y;
    for (const BlockPos& log : logs) {
        if (log.y - treeY > 2) {
            continue;
        }
        for (const CocoaPlacement& cocoa : kCocoaDirections) {
            if (random.nextFloat() > 0.25F) {
                continue;
            }
            const std::int32_t x = log.x + cocoa.dx;
            const std::int32_t z = log.z + cocoa.dz;
            if (isAir(chunk, x, log.y, z)) {
                const std::int32_t age = random.nextInt(3);
                setBlock(chunk, x, log.y, z, stateIdOf("cocoa") + age * 4 + cocoa.facingOffset);
            }
        }
    }
}

void decoratePaleMoss(Chunk& chunk, random::RandomSource& random, random::RandomSource& mossyCarpetRandom,
                      const TreeDecoratorContext& context, const decorator::PaleMoss& config) {
    std::vector<BlockPos> shuffledLogs = context.logs;
    treeDecorators::shuffle(shuffledLogs, random);
    if (shuffledLogs.empty()) {
        return;
    }
    const BlockPos origin = *std::min_element(shuffledLogs.begin(), shuffledLogs.end(),
                                              [](const BlockPos& a, const BlockPos& b) { return a.y < b.y; });
    if (random.nextFloat() < config.groundProbability) {
        paleMossDecoration::placePatchAt(chunk, random, DecorationPos{.x = origin.x, .y = origin.y + 1, .z = origin.z},
                                         mossyCarpetRandom);
    }
    for (const BlockPos& log : context.logs) {
        if (random.nextFloat() < config.trunkProbability && isAir(chunk, log.x, log.y - 1, log.z)) {
            addPaleMossHanger(chunk, random, log.x, log.y - 1, log.z);
        }
    }
    for (const BlockPos& leaf : context.leaves) {
        if (random.nextFloat() < config.leavesProbability && isAir(chunk, leaf.x, leaf.y - 1, leaf.z)) {
            addPaleMossHanger(chunk, random, leaf.x, leaf.y - 1, leaf.z);
        }
    }
}

void decorateCreakingHeart(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& logs,
                           float probability) {
    if (logs.empty() || random.nextFloat() >= probability) {
        return;
    }
    std::vector<BlockPos> candidates = logs;
    treeDecorators::shuffle(candidates, random);
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t heartStateId = stateIdOf("creaking_heart") + 1;
    for (const BlockPos& position : candidates) {
        const bool surrounded = std::ranges::all_of(kBlockNeighbours, [&](const std::array<std::int32_t, 3>& offset) {
            const std::int32_t y = position.y + offset[1];
            if (y < bounds.minY() || y > bounds.maxY()) {
                return false;
            }
            return treeBlockRules::logBlockPaths().contains(
                pathOf(chunk.decorationBlockStateId(position.x + offset[0], y, position.z + offset[2])));
        });
        if (!surrounded) {
            continue;
        }
        setBlock(chunk, position.x, position.y, position.z, heartStateId);
        chunk.setDecorationBlockEntity(position.x, position.y, position.z, heartStateId,
                                       wire::common::Identifier::vanilla("creaking_heart"), wire::nbt::NbtCompound{});
        return;
    }
}

/// parity: `AttachedToLeavesDecorator`, used by both mangrove tree configurations.
void decorateMangrovePropagules(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& leaves,
                                const decorator::MangrovePropagules& config) {
    std::set<BlockPos> blacklist;
    std::vector<BlockPos> candidates = leaves;
    treeDecorators::shuffle(candidates, random);
    const std::int32_t age0StateId = stateIdOf("mangrove_propagule") - 4;
    for (const BlockPos& leaf : candidates) {
        // parity: Util.getRandom over the single DOWN direction still draws.
        static_cast<void>(random.nextInt(1));
        const BlockPos placement{.x = leaf.x, .y = leaf.y - 1, .z = leaf.z};
        if (blacklist.contains(placement) || random.nextFloat() >= config.probability) {
            continue;
        }
        bool blocked = false;
        for (std::int32_t depth = 1; depth <= config.requiredEmptyBlocks; ++depth) {
            if (!isAir(chunk, leaf.x, leaf.y - depth, leaf.z)) {
                blocked = true;
                break;
            }
        }
        if (blocked) {
            continue;
        }
        for (std::int32_t x = placement.x - config.exclusionRadiusXZ; x <= placement.x + config.exclusionRadiusXZ;
             ++x) {
            for (std::int32_t y = placement.y - config.exclusionRadiusY; y <= placement.y + config.exclusionRadiusY;
                 ++y) {
                for (std::int32_t z = placement.z - config.exclusionRadiusXZ;
                     z <= placement.z + config.exclusionRadiusXZ; ++z) {
                    blacklist.insert(BlockPos{.x = x, .y = y, .z = z});
                }
            }
        }
        const std::int32_t age = random.nextInt(5);
        setBlock(chunk, placement.x, placement.y, placement.z, age0StateId + age * 8);
    }
}

/// parity: `PlaceOnGroundDecorator` with `VegetationFeatures.leafLitterPatchBuilder`.
void decorateGroundLeafLitter(Chunk& chunk, random::RandomSource& random, const TreeDecoratorContext& context,
                              const decorator::GroundLeafLitter& config) {
    const std::vector<BlockPos> supportPositions = lowestTrunkOrRootPositions(context);
    if (supportPositions.empty()) {
        return;
    }
    const std::int32_t minY = supportPositions.front().y;
    std::int32_t minX = supportPositions.front().x;
    std::int32_t maxX = minX;
    std::int32_t minZ = supportPositions.front().z;
    std::int32_t maxZ = minZ;
    for (const BlockPos& position : supportPositions) {
        if (position.y != minY) {
            continue;
        }
        minX = std::min(minX, position.x);
        maxX = std::max(maxX, position.x);
        minZ = std::min(minZ, position.z);
        maxZ = std::max(maxZ, position.z);
    }
    minX -= config.radius;
    maxX += config.radius;
    minZ -= config.radius;
    maxZ += config.radius;
    const std::int32_t minSampleY = minY - config.height;
    const std::int32_t maxSampleY = minY + config.height;
    const ChunkHeight bounds = chunk.verticalBounds();

    for (std::int32_t attempt = 0; attempt < config.tries; ++attempt) {
        const std::int32_t x = minX + random.nextInt(maxX - minX + 1);
        const std::int32_t y = minSampleY + random.nextInt(maxSampleY - minSampleY + 1);
        const std::int32_t z = minZ + random.nextInt(maxZ - minZ + 1);
        const std::int32_t aboveY = y + 1;
        if (y < bounds.minY() || y > bounds.maxY() || aboveY < bounds.minY() || aboveY > bounds.maxY()) {
            continue;
        }
        const std::int32_t aboveStateId = chunk.decorationBlockStateId(x, aboveY, z);
        if (!registry::blocks::isAir(aboveStateId) && !registry::blockStateFamilies::isVineState(aboveStateId)) {
            continue;
        }
        if (!registry::blockShapes::isSolidRender(chunk.decorationBlockStateId(x, y, z))) {
            continue;
        }
        if (decorationHeightAt(chunk, x, z, HeightmapType::MotionBlockingNoLeaves) > aboveY) {
            continue;
        }
        const std::int32_t choice = random.nextInt(config.maxSegmentAmount * 4);
        const std::int32_t segmentOffset = choice / 4;
        const std::int32_t facingOffset = kLeafLitterFacingStateOffsets[static_cast<std::size_t>(choice % 4)];
        setBlock(chunk, x, aboveY, z, stateIdOf("leaf_litter") + facingOffset + segmentOffset);
    }
}

/// parity: `BeehiveDecorator` placement, occupant data and RNG.
void decorateBeehive(Chunk& chunk, random::RandomSource& random, const TreeDecoratorContext& context,
                     float probability) {
    if (context.logs.empty() || random.nextFloat() >= probability) {
        return;
    }
    const std::int32_t hiveY = context.leaves.empty()
                                   ? std::min(context.logs.front().y + 1 + random.nextInt(3), context.logs.back().y)
                                   : std::max(context.leaves.front().y - 1, context.logs.front().y + 1);
    std::vector<BlockPos> placements;
    for (const BlockPos& log : context.logs) {
        if (log.y != hiveY) {
            continue;
        }
        for (const auto& [dx, dz] : kBeehiveSpawnDirections) {
            placements.push_back(BlockPos{.x = log.x + dx, .y = hiveY, .z = log.z + dz});
        }
    }
    if (placements.empty()) {
        return;
    }
    treeDecorators::shuffle(placements, random);
    const auto hive = std::ranges::find_if(placements, [&chunk](const BlockPos& position) {
        return isAir(chunk, position.x, position.y, position.z) && isAir(chunk, position.x, position.y, position.z + 1);
    });
    if (hive == placements.end()) {
        return;
    }
    const std::int32_t nestStateId = stateIdOf("bee_nest") + 6;
    setBlock(chunk, hive->x, hive->y, hive->z, nestStateId);
    const std::int32_t beeCount = 2 + random.nextInt(2);
    std::vector<wire::nbt::NbtTag> bees;
    for (std::int32_t bee = 0; bee < beeCount; ++bee) {
        wire::nbt::NbtCompound entityData;
        entityData.putString("id", "minecraft:bee");
        wire::nbt::NbtCompound occupant;
        occupant.set("entity_data", wire::nbt::NbtTag::ofCompound(std::move(entityData)));
        occupant.putInt("ticks_in_hive", random.nextInt(599));
        occupant.putInt("min_ticks_in_hive", 600);
        bees.push_back(wire::nbt::NbtTag::ofCompound(std::move(occupant)));
    }
    wire::nbt::NbtCompound data;
    data.set("bees", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(bees)}));
    chunk.setDecorationBlockEntity(hive->x, hive->y, hive->z, nestStateId, wire::common::Identifier::vanilla("beehive"),
                                   std::move(data));
}

} // namespace

void treeDecorators::shuffle(std::vector<BlockPos>& values, random::RandomSource& random) {
    for (std::size_t size = values.size(); size >= 2; --size) {
        const std::size_t swapTo = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(size)));
        std::swap(values[size - 1], values[swapTo]);
    }
}

void treeDecorators::decorate(Chunk& chunk, random::RandomSource& random, random::RandomSource& mossyCarpetRandom,
                              const TreeDecoratorContext& context, const TreeDecoratorConfiguration& decorator) {
    std::visit(
        [&]<typename Config>(const Config& config) {
            if constexpr (std::is_same_v<Config, decorator::AlterGroundToPodzol>) {
                alterGroundToPodzol(chunk, random, context.logs, context.roots);
            } else if constexpr (std::is_same_v<Config, decorator::TrunkVines>) {
                decorateTrunkVines(chunk, random, context.logs);
            } else if constexpr (std::is_same_v<Config, decorator::LeafVines>) {
                decorateLeafVines(chunk, random, context.leaves, config.probability);
            } else if constexpr (std::is_same_v<Config, decorator::Cocoa>) {
                decorateCocoa(chunk, random, context.logs, config.probability);
            } else if constexpr (std::is_same_v<Config, decorator::PaleMoss>) {
                decoratePaleMoss(chunk, random, mossyCarpetRandom, context, config);
            } else if constexpr (std::is_same_v<Config, decorator::CreakingHeart>) {
                decorateCreakingHeart(chunk, random, context.logs, config.probability);
            } else if constexpr (std::is_same_v<Config, decorator::MangrovePropagules>) {
                decorateMangrovePropagules(chunk, random, context.leaves, config);
            } else if constexpr (std::is_same_v<Config, decorator::GroundLeafLitter>) {
                decorateGroundLeafLitter(chunk, random, context, config);
            } else {
                decorateBeehive(chunk, random, context, config.probability);
            }
        },
        decorator);
}

} // namespace basalt::server::world::terrain::decoration::tree

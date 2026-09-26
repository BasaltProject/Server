#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/JavaHashSetOrder.hpp>
#include <basalt/server/world/terrain/decoration/tree/FallenTreeFeature.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeatureDecorators.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

constexpr float kAttachedMushroomProbability = 0.1F;

struct FallDirection {
    std::int32_t dx = 0;
    std::int32_t dz = 0;
    bool axisX = false;
};

struct StumpVine {
    std::int32_t dx = 0;
    std::int32_t dz = 0;
    std::int32_t faceBit = 0;
};

struct LogPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

constexpr std::array<FallDirection, 4> kFallDirections{
    FallDirection{.dx = 0, .dz = -1, .axisX = false}, FallDirection{.dx = 1, .dz = 0, .axisX = true},
    FallDirection{.dx = 0, .dz = 1, .axisX = false}, FallDirection{.dx = -1, .dz = 0, .axisX = true}};

constexpr std::array<StumpVine, 4> kStumpVines{
    StumpVine{.dx = -1, .dz = 0, .faceBit = 16}, StumpVine{.dx = 1, .dz = 0, .faceBit = 1},
    StumpVine{.dx = 0, .dz = -1, .faceBit = 4}, StumpVine{.dx = 0, .dz = 1, .faceBit = 8}};

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    return y >= bounds.minY() && y <= bounds.maxY() && registry::blocks::isAir(chunk.decorationBlockStateId(x, y, z));
}

[[nodiscard]] bool isOverSolidGround(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    return y > chunk.verticalBounds().minY() &&
           registry::blockShapes::isFaceSturdy(chunk.decorationBlockStateId(x, y - 1, z), registry::BlockFace::Up);
}

[[nodiscard]] bool mayPlaceOn(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    return isValidTreePos(chunk, x, y, z) && isOverSolidGround(chunk, x, y, z);
}

/// The bucket iteration order of vanilla's `HashSet<BlockPos>`, which the mushroom shuffle starts

void decorateStumpVines(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const StumpVine& vine : kStumpVines) {
        if (random.nextInt(3) > 0 && isAir(chunk, x + vine.dx, y, z + vine.dz)) {
            setBlock(chunk, x + vine.dx, y, z + vine.dz, state("vine") - vine.faceBit);
        }
    }
}

void decorateAttachedMushrooms(Chunk& chunk, random::RandomSource& random, const std::vector<LogPos>& logs,
                               bool brownOnly) {
    std::vector<LogPos> shuffled = javaHashSetOrder(logs);
    for (std::size_t size = shuffled.size(); size >= 2; --size) {
        const std::size_t swapTo = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(size)));
        std::swap(shuffled[size - 1], shuffled[swapTo]);
    }
    for (const LogPos& log : shuffled) {
        // parity: Util.getRandom over the single UP direction still draws.
        static_cast<void>(random.nextInt(1));
        if (random.nextFloat() > kAttachedMushroomProbability || !isAir(chunk, log.x, log.y + 1, log.z)) {
            continue;
        }
        setBlock(chunk, log.x, log.y + 1, log.z,
                 !brownOnly && random.nextInt(3) < 2 ? state("red_mushroom") : state("brown_mushroom"));
    }
}

bool placeConfigured(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                     std::int32_t originZ, const FallenTreeConfiguration& config) {
    setBlock(chunk, originX, originY, originZ, config.logStateId);
    // parity: `placeLogBlock` marks the two blocks over every log for post-processing.
    chunk.markAboveForPostProcessing(originX, originY, originZ);
    if (config.stumpVines) {
        decorateStumpVines(chunk, random, originX, originY, originZ);
    }
    const FallDirection direction = kFallDirections[static_cast<std::size_t>(random.nextInt(4))];
    const std::int32_t logLength = config.logLength.sample(random) - 2;
    const std::int32_t distanceFromStump = 2 + random.nextInt(2);
    const std::int32_t startX = originX + direction.dx * distanceFromStump;
    std::int32_t startY = originY + 1;
    const std::int32_t startZ = originZ + direction.dz * distanceFromStump;
    for (std::int32_t probe = 0; probe < 6; ++probe) {
        if (mayPlaceOn(chunk, startX, startY, startZ)) {
            break;
        }
        startY--;
    }

    std::int32_t gap = 0;
    for (std::int32_t index = 0; index < logLength; ++index) {
        const std::int32_t x = startX + direction.dx * index;
        const std::int32_t z = startZ + direction.dz * index;
        if (!isValidTreePos(chunk, x, startY, z)) {
            return true;
        }
        if (isOverSolidGround(chunk, x, startY, z)) {
            gap = 0;
        } else if (++gap > 2) {
            return true;
        }
    }
    const std::int32_t sidewaysState = config.logStateId + (direction.axisX ? -1 : 1);
    std::vector<LogPos> fallenLogs;
    fallenLogs.reserve(static_cast<std::size_t>(logLength));
    for (std::int32_t index = 0; index < logLength; ++index) {
        const LogPos log{.x = startX + direction.dx * index, .y = startY, .z = startZ + direction.dz * index};
        setBlock(chunk, log.x, log.y, log.z, sidewaysState);
        chunk.markAboveForPostProcessing(log.x, log.y, log.z);
        fallenLogs.push_back(log);
    }
    decorateAttachedMushrooms(chunk, random, fallenLogs, config.brownMushroomsOnly);
    if (config.shelfMushroom.has_value()) {
        // parity: the decorator context keeps the log set's hash order, all logs sharing one y.
        std::vector<BlockPos> logs;
        for (const LogPos& log : javaHashSetOrder(fallenLogs)) {
            logs.push_back(BlockPos{.x = log.x, .y = log.y, .z = log.z});
        }
        treeDecorators::decorateShelfMushroom(chunk, random, logs, *config.shelfMushroom);
    }
    return true;
}

} // namespace

const FallenTreeConfiguration& fallenTreeConfigs::oak() {
    static const FallenTreeConfiguration config{
        .logStateId = state("oak_log"), .logLength = IntProvider::uniform(4, 7), .stumpVines = true};
    return config;
}

const FallenTreeConfiguration& fallenTreeConfigs::birch() {
    static const FallenTreeConfiguration config{.logStateId = state("birch_log"),
                                                .logLength = IntProvider::uniform(5, 8)};
    return config;
}

const FallenTreeConfiguration& fallenTreeConfigs::superBirch() {
    static const FallenTreeConfiguration config{.logStateId = state("birch_log"),
                                                .logLength = IntProvider::uniform(5, 15)};
    return config;
}

const FallenTreeConfiguration& fallenTreeConfigs::spruce() {
    static const FallenTreeConfiguration config{.logStateId = state("spruce_log"),
                                                .logLength = IntProvider::uniform(6, 10)};
    return config;
}

const FallenTreeConfiguration& fallenTreeConfigs::jungle() {
    static const FallenTreeConfiguration config{
        .logStateId = state("jungle_log"), .logLength = IntProvider::uniform(4, 11), .stumpVines = true};
    return config;
}

const FallenTreeConfiguration& fallenTreeConfigs::poplar() {
    static const FallenTreeConfiguration config{.logStateId = state("poplar_log"),
                                                .logLength = IntProvider::uniform(4, 7),
                                                .brownMushroomsOnly = true,
                                                .shelfMushroom = 0.8F};
    return config;
}

bool fallenTreeFeature::place(Chunk& chunk, random::RandomSource& random, const std::int32_t originX,
                              const std::int32_t originY, const std::int32_t originZ,
                              const FallenTreeConfiguration& config) {
    const Chunk::WritePolicyScope policy{chunk, GeneratedBlockWritePolicy::Tree};
    return placeConfigured(chunk, random, originX, originY, originZ, config);
}

} // namespace basalt::server::world::terrain::decoration::tree

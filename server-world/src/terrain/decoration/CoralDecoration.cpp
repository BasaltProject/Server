#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CoralDecoration.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;

struct Direction {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;

    [[nodiscard]] Direction clockwise() const { return Direction{-z, y, x}; }

    [[nodiscard]] Direction counterClockwise() const { return Direction{z, y, -x}; }

    [[nodiscard]] Direction opposite() const { return Direction{-x, -y, -z}; }

    [[nodiscard]] std::string_view horizontalFacing() const {
        if (z < 0) {
            return "north";
        }
        if (x > 0) {
            return "east";
        }
        return z > 0 ? "south" : "west";
    }

    friend bool operator==(const Direction&, const Direction&) = default;
};

constexpr Direction kUp{0, 1, 0};
constexpr std::array<Direction, 4> kHorizontalDirections{Direction{0, 0, -1}, Direction{1, 0, 0}, Direction{0, 0, 1},
                                                         Direction{-1, 0, 0}};
constexpr std::array<std::string_view, 5> kCoralNames{"tube", "brain", "bubble", "fire", "horn"};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::vector<std::int32_t> blockStates(std::string_view suffix) {
    std::vector<std::int32_t> built;
    built.reserve(kCoralNames.size());
    for (const std::string_view name : kCoralNames) {
        built.push_back(stateIdOf(std::string{name} + std::string{suffix}));
    }
    return built;
}

const std::vector<std::int32_t>& coralBlocks() {
    static const std::vector<std::int32_t> states = blockStates("_coral_block");
    return states;
}

const std::vector<std::int32_t>& corals() {
    static const std::vector<std::int32_t> states = [] {
        std::vector<std::int32_t> built = blockStates("_coral");
        const std::vector<std::int32_t> fans = blockStates("_coral_fan");
        built.insert(built.end(), fans.begin(), fans.end());
        return built;
    }();
    return states;
}

const std::vector<std::int32_t>& wallCorals() {
    static const std::vector<std::int32_t> states = blockStates("_coral_wall_fan");
    return states;
}

const std::set<std::string, std::less<>>& coralTargetPaths() {
    static const std::set<std::string, std::less<>> paths = [] {
        std::set<std::string, std::less<>> built;
        for (const std::string_view name : kCoralNames) {
            built.insert(std::string{name} + "_coral");
            built.insert(std::string{name} + "_coral_fan");
        }
        return built;
    }();
    return paths;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] bool isWaterBlock(std::int32_t stateId) { return pathOf(stateId) == "water"; }

[[nodiscard]] bool isCoralTarget(std::int32_t stateId) { return coralTargetPaths().contains(pathOf(stateId)); }

[[nodiscard]] DecorationPos move(const DecorationPos& position, const Direction& direction) {
    return DecorationPos{.x = position.x + direction.x, .y = position.y + direction.y, .z = position.z + direction.z};
}

[[nodiscard]] DecorationPos up(const DecorationPos& position) {
    return DecorationPos{.x = position.x, .y = position.y + 1, .z = position.z};
}

[[nodiscard]] bool inWorld(const Chunk& chunk, const DecorationPos& position) {
    const ChunkHeight bounds = chunk.verticalBounds();
    return position.y >= bounds.minY() && position.y <= bounds.maxY();
}

/// Outside the source chunk unknown air reads as water, which is what an ocean neighbour holds.
[[nodiscard]] std::int32_t blockAt(const Chunk& chunk, const DecorationPos& position) {
    const std::int32_t stateId = chunk.decorationBlockStateId(position.x, position.y, position.z);
    const bool outsideSourceChunk = (position.x >> 4) != chunk.chunkX() || (position.z >> 4) != chunk.chunkZ();
    return outsideSourceChunk && stateId == registry::blocks::air().stateId ? registry::blocks::water().stateId
                                                                            : stateId;
}

void setBlock(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId, registry::blocks::water().stateId);
}

void shuffle(std::vector<Direction>& values, random::RandomSource& random) {
    for (auto size = static_cast<std::int32_t>(values.size()); size >= 2; --size) {
        const std::int32_t swapTo = random.nextInt(size);
        std::swap(values[static_cast<std::size_t>(size - 1)], values[static_cast<std::size_t>(swapTo)]);
    }
}

bool placeCoralBlock(Chunk& chunk, random::RandomSource& random, const DecorationPos& position, std::int32_t stateId) {
    if (!inWorld(chunk, position) || !inWorld(chunk, up(position))) {
        return false;
    }
    const std::int32_t target = blockAt(chunk, position);
    if ((!isWaterBlock(target) && !isCoralTarget(target)) || !isWaterBlock(blockAt(chunk, up(position)))) {
        return false;
    }

    setBlock(chunk, position, stateId);
    if (random.nextFloat() < 0.25F) {
        setBlock(chunk, up(position),
                 corals()[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(corals().size())))]);
    } else if (random.nextFloat() < 0.05F) {
        setBlock(chunk, up(position), registry::blocks::named("sea_pickle")->stateId + random.nextInt(4) * 2);
    }

    for (const Direction& direction : kHorizontalDirections) {
        if (random.nextFloat() >= 0.2F) {
            continue;
        }
        const DecorationPos side = move(position, direction);
        if (!isWaterBlock(blockAt(chunk, side))) {
            continue;
        }
        const std::int32_t wallCoralStateId =
            wallCorals()[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(wallCorals().size())))];
        const std::optional<std::int32_t> fan =
            registry::blockStateFamilies::coralWallFanStateId(wallCoralStateId, direction.horizontalFacing(), true);
        if (!fan.has_value()) {
            throw std::runtime_error{"invalid wall coral state"};
        }
        setBlock(chunk, side, *fan);
    }
    return true;
}

void placeTree(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t stateId) {
    DecorationPos position = origin;
    const std::int32_t trunkHeight = random.nextInt(3) + 1;
    for (std::int32_t step = 0; step < trunkHeight; ++step) {
        if (!placeCoralBlock(chunk, random, position, stateId)) {
            return;
        }
        position = up(position);
    }

    const DecorationPos trunkTop = position;
    const std::int32_t branchCount = random.nextInt(3) + 2;
    std::vector<Direction> directions{kHorizontalDirections.begin(), kHorizontalDirections.end()};
    shuffle(directions, random);
    for (std::int32_t branch = 0; branch < branchCount && branch < static_cast<std::int32_t>(directions.size());
         ++branch) {
        const Direction& direction = directions[static_cast<std::size_t>(branch)];
        position = move(trunkTop, direction);
        const std::int32_t branchHeight = random.nextInt(5) + 2;
        std::int32_t segmentLength = 0;
        for (std::int32_t index = 0; index < branchHeight; ++index) {
            if (!placeCoralBlock(chunk, random, position, stateId)) {
                break;
            }
            ++segmentLength;
            position = up(position);
            if (index == 0 || (segmentLength >= 2 && random.nextFloat() < 0.25F)) {
                position = move(position, direction);
                segmentLength = 0;
            }
        }
    }
}

void placeClaw(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t stateId) {
    if (!placeCoralBlock(chunk, random, origin, stateId)) {
        return;
    }

    const Direction clawDirection = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
    const std::int32_t branchCount = random.nextInt(2) + 2;
    std::vector<Direction> possibleDirections{clawDirection, clawDirection.clockwise(),
                                              clawDirection.counterClockwise()};
    shuffle(possibleDirections, random);
    for (std::int32_t branch = 0; branch < branchCount && branch < static_cast<std::int32_t>(possibleDirections.size());
         ++branch) {
        const Direction& branchDirection = possibleDirections[static_cast<std::size_t>(branch)];
        DecorationPos position = move(origin, branchDirection);
        const std::int32_t sidewaysLength = random.nextInt(2) + 1;
        std::int32_t inwardLength = 0;
        Direction segmentDirection{};
        if (branchDirection == clawDirection) {
            segmentDirection = clawDirection;
            inwardLength = random.nextInt(3) + 2;
        } else {
            position = up(position);
            segmentDirection = random.nextInt(2) == 0 ? branchDirection : kUp;
            inwardLength = random.nextInt(3) + 3;
        }

        for (std::int32_t index = 0; index < sidewaysLength; ++index) {
            if (!placeCoralBlock(chunk, random, position, stateId)) {
                break;
            }
            position = move(position, segmentDirection);
        }
        position = up(move(position, segmentDirection.opposite()));

        for (std::int32_t index = 0; index < inwardLength; ++index) {
            position = move(position, clawDirection);
            if (!placeCoralBlock(chunk, random, position, stateId)) {
                break;
            }
            if (random.nextFloat() < 0.25F) {
                position = up(position);
            }
        }
    }
}

void placeMushroom(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t stateId) {
    const std::int32_t height = random.nextInt(3) + 3;
    const std::int32_t width = random.nextInt(3) + 3;
    const std::int32_t length = random.nextInt(3) + 3;
    const std::int32_t sink = random.nextInt(3) + 1;
    for (std::int32_t x = 0; x <= width; ++x) {
        for (std::int32_t y = 0; y <= height; ++y) {
            for (std::int32_t z = 0; z <= length; ++z) {
                const bool xEdge = x == 0 || x == width;
                const bool yEdge = y == 0 || y == height;
                const bool zEdge = z == 0 || z == length;
                const bool shell =
                    (!xEdge || !yEdge) && (!zEdge || !yEdge) && (!xEdge || !zEdge) && (xEdge || yEdge || zEdge);
                if (shell && random.nextFloat() >= 0.1F) {
                    placeCoralBlock(chunk, random,
                                    DecorationPos{.x = origin.x + x, .y = origin.y + y - sink, .z = origin.z + z},
                                    stateId);
                }
            }
        }
    }
}

const std::vector<PlacementModifier>& warmOceanModifiers() {
    static const std::vector<PlacementModifier> modifiers = {noiseBasedCountPlacement(20, 400.0), inSquarePlacement(),
                                                             worldgenHeightmapPlacement(HeightmapType::OceanFloor)};
    return modifiers;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

} // namespace

const std::set<std::string, std::less<>>& coralDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"warm_ocean_vegetation"};
    return ids;
}

void coralDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    // parity: the shape is drawn first and the colour second, in that order.
    const std::int32_t feature = random.nextInt(3);
    const std::int32_t coralState =
        coralBlocks()[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(coralBlocks().size())))];
    if (feature == 0) {
        placeTree(chunk, random, origin, coralState);
    } else if (feature == 1) {
        placeClaw(chunk, random, origin, coralState);
    } else {
        placeMushroom(chunk, random, origin, coralState);
    }
}

BiomeVegetalDecoration coralDecoration::vegetalDecoration() {
    return [](Chunk& chunk, random::RandomSource& random) {
        const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
        placeFrom(chunk, random, chunkOrigin, warmOceanModifiers(),
                  [&](const DecorationPos& origin) { placeAt(chunk, random, origin); });
    };
}

void coralDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                               const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kWarmOceanVegetationIndex, kVegetalDecorationStep);
    const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    placeFrom(chunk, random, chunkOrigin, warmOceanModifiers(), [&](const DecorationPos& origin) {
        if (biomePath(environment.biomeAt(origin.x, origin.y, origin.z)) != "warm_ocean") {
            return;
        }
        placeAt(chunk, random, origin);
    });
}

} // namespace basalt::server::world::terrain::decoration

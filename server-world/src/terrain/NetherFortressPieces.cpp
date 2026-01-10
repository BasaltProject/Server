#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/NetherFortressPieces.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::netherFortressPieces {
namespace {

using structure::StructureBoundingBox;
using structure::StructureDirection;
using structure::StructurePos;

constexpr std::array<StructureDirection, 4> kHorizontalDirections{StructureDirection::North, StructureDirection::East,
                                                                  StructureDirection::South, StructureDirection::West};

[[nodiscard]] std::int32_t plainState(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

struct Dimensions {
    std::int32_t offX = 0;
    std::int32_t offY = 0;
    std::int32_t offZ = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t depth = 0;
};

[[nodiscard]] Dimensions dimensionsOf(PieceType type) {
    switch (type) {
    case PieceType::BridgeCrossing:
    case PieceType::Start:
        return {-8, -3, 0, 19, 10, 19};
    case PieceType::BridgeEndFiller:
        return {-1, -3, 0, 5, 10, 8};
    case PieceType::BridgeStraight:
        return {-1, -3, 0, 5, 10, 19};
    case PieceType::CastleStairs:
        return {-1, -7, 0, 5, 14, 10};
    case PieceType::CastleTBalcony:
        return {-3, 0, 0, 9, 7, 9};
    case PieceType::CastleEntrance:
        return {-5, -3, 0, 13, 14, 13};
    case PieceType::CastleSmallCrossing:
    case PieceType::CastleLeftTurn:
    case PieceType::CastleSmallCorridor:
    case PieceType::CastleRightTurn:
        return {-1, 0, 0, 5, 7, 5};
    case PieceType::CastleStalkRoom:
        return {-5, -3, 0, 13, 14, 13};
    case PieceType::MonsterThrone:
        return {-2, 0, 0, 7, 8, 9};
    case PieceType::RoomCrossing:
        return {-2, 0, 0, 7, 9, 7};
    case PieceType::StairsRoom:
    default:
        return {-2, 0, 0, 7, 11, 7};
    }
}

struct PieceWeight {
    PieceType type = PieceType::BridgeStraight;
    std::int32_t weight = 0;
    std::int32_t maxCount = 0;
    bool allowInRow = false;
    std::int32_t count = 0;
};

[[nodiscard]] std::vector<PieceWeight> bridgeWeights() {
    return {{PieceType::BridgeStraight, 30, 0, true}, {PieceType::BridgeCrossing, 10, 4},
            {PieceType::RoomCrossing, 10, 4},         {PieceType::StairsRoom, 10, 3},
            {PieceType::MonsterThrone, 5, 2},         {PieceType::CastleEntrance, 5, 1}};
}

[[nodiscard]] std::vector<PieceWeight> castleWeights() {
    return {{PieceType::CastleSmallCorridor, 25, 0, true},
            {PieceType::CastleSmallCrossing, 15, 5},
            {PieceType::CastleRightTurn, 5, 10},
            {PieceType::CastleLeftTurn, 5, 10},
            {PieceType::CastleStairs, 10, 3, true},
            {PieceType::CastleTBalcony, 7, 2},
            {PieceType::CastleStalkRoom, 5, 2}};
}

[[nodiscard]] StructureBoundingBox orientBox(std::int32_t x, std::int32_t y, std::int32_t z, const Dimensions& d,
                                             StructureDirection direction) {
    switch (direction) {
    case StructureDirection::North:
        return StructureBoundingBox{
            x + d.offX, y + d.offY, z - d.depth + 1 + d.offZ, x + d.width - 1 + d.offX, y + d.height - 1 + d.offY,
            z + d.offZ};
    case StructureDirection::West:
        return StructureBoundingBox{
            x - d.depth + 1 + d.offZ, y + d.offY, z + d.offX, x + d.offZ, y + d.height - 1 + d.offY,
            z + d.width - 1 + d.offX};
    case StructureDirection::East:
        return StructureBoundingBox{x + d.offZ,
                                    y + d.offY,
                                    z + d.offX,
                                    x + d.depth - 1 + d.offZ,
                                    y + d.height - 1 + d.offY,
                                    z + d.width - 1 + d.offX};
    default:
        return StructureBoundingBox{x + d.offX,
                                    y + d.offY,
                                    z + d.offZ,
                                    x + d.width - 1 + d.offX,
                                    y + d.height - 1 + d.offY,
                                    z + d.depth - 1 + d.offZ};
    }
}

[[nodiscard]] StructureBoundingBox makeBoundingBox(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   StructureDirection direction, std::int32_t width,
                                                   std::int32_t height, std::int32_t depth) {
    if (direction == StructureDirection::North || direction == StructureDirection::South) {
        return StructureBoundingBox{x, y, z, x + width - 1, y + height - 1, z + depth - 1};
    }
    return StructureBoundingBox{x, y, z, x + depth - 1, y + height - 1, z + width - 1};
}

/// The assembly's whole working state: the pieces so far, which still owe children, and the two
/// weight tables, each of which counts down as pieces of a kind are used up.
struct AssemblyState {
    std::vector<Piece> pieces;
    /// Indices into `pieces`, since the vector grows while the queue is walked.
    std::vector<std::size_t> pending;
    std::vector<PieceWeight> bridge = bridgeWeights();
    std::vector<PieceWeight> castle = castleWeights();
    /// The last weight that produced a piece, which most kinds may not follow themselves.
    std::optional<PieceType> previous;
    StructureBoundingBox startBox;
};

void addChildren(AssemblyState& state, std::size_t index, random::RandomSource& random);

[[nodiscard]] std::optional<Piece> createPiece(PieceType type, AssemblyState& state, random::RandomSource& random,
                                               std::int32_t x, std::int32_t y, std::int32_t z,
                                               StructureDirection direction, std::int32_t depth) {
    const StructureBoundingBox box = orientBox(x, y, z, dimensionsOf(type), direction);
    if (box.minY <= 10) {
        return std::nullopt;
    }
    for (const Piece& placed : state.pieces) {
        if (placed.boundingBox.intersects(box)) {
            return std::nullopt;
        }
    }
    Piece built{.type = type, .generationDepth = depth, .boundingBox = box, .orientation = direction};
    if (type == PieceType::BridgeEndFiller) {
        built.selfSeed = random.nextInt();
    }
    if (type == PieceType::CastleLeftTurn || type == PieceType::CastleRightTurn) {
        built.needsChest = random.nextInt(3) == 0;
    }
    return built;
}

[[nodiscard]] std::optional<Piece> generatePiece(AssemblyState& state, std::vector<PieceWeight>& available,
                                                 random::RandomSource& random, std::int32_t x, std::int32_t y,
                                                 std::int32_t z, StructureDirection direction, std::int32_t depth) {
    const bool hasLimited = std::any_of(available.begin(), available.end(), [](const PieceWeight& weight) {
        return weight.maxCount > 0 && weight.count < weight.maxCount;
    });
    std::int32_t totalWeight = -1;
    if (hasLimited) {
        totalWeight = 0;
        for (const PieceWeight& weight : available) {
            totalWeight += weight.weight;
        }
    }
    for (std::int32_t attempt = 0; attempt < 5; ++attempt) {
        if (totalWeight <= 0 || depth > 30) {
            continue;
        }
        std::int32_t selection = random.nextInt(totalWeight);
        for (std::size_t index = 0; index < available.size(); ++index) {
            PieceWeight& weight = available[index];
            selection -= weight.weight;
            if (selection >= 0) {
                continue;
            }
            if ((weight.maxCount > 0 && weight.count >= weight.maxCount) ||
                (state.previous.has_value() && *state.previous == weight.type && !weight.allowInRow)) {
                break;
            }
            const std::optional<Piece> piece = createPiece(weight.type, state, random, x, y, z, direction, depth);
            // parity: a refused kind does not end the walk. The running selection is already
            // negative, so every weight after it is tried in turn.
            if (piece.has_value()) {
                ++weight.count;
                state.previous = weight.type;
                if (weight.maxCount > 0 && weight.count >= weight.maxCount) {
                    available.erase(available.begin() + static_cast<std::ptrdiff_t>(index));
                }
                return piece;
            }
        }
    }
    return createPiece(PieceType::BridgeEndFiller, state, random, x, y, z, direction, depth);
}

void addPiece(AssemblyState& state, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
              StructureDirection direction, bool castle, std::int32_t parentDepth) {
    if (std::abs(x - state.startBox.minX) > 112 || std::abs(z - state.startBox.minZ) > 112) {
        // parity: the jar builds the filler and throws it away. Only its draw survives, and the
        // fortress simply stops growing in that direction.
        static_cast<void>(createPiece(PieceType::BridgeEndFiller, state, random, x, y, z, direction, parentDepth));
        return;
    }
    std::vector<PieceWeight>& available = castle ? state.castle : state.bridge;
    const std::optional<Piece> piece = generatePiece(state, available, random, x, y, z, direction, parentDepth + 1);
    if (!piece.has_value()) {
        return;
    }
    state.pieces.push_back(*piece);
    state.pending.push_back(state.pieces.size() - 1);
}

void childForward(AssemblyState& state, random::RandomSource& random, const StructureBoundingBox& box,
                  StructureDirection orientation, std::int32_t depth, std::int32_t xOffset, std::int32_t yOffset,
                  bool castle) {
    switch (orientation) {
    case StructureDirection::North:
        addPiece(state, random, box.minX + xOffset, box.minY + yOffset, box.minZ - 1, orientation, castle, depth);
        break;
    case StructureDirection::South:
        addPiece(state, random, box.minX + xOffset, box.minY + yOffset, box.maxZ + 1, orientation, castle, depth);
        break;
    case StructureDirection::West:
        addPiece(state, random, box.minX - 1, box.minY + yOffset, box.minZ + xOffset, orientation, castle, depth);
        break;
    case StructureDirection::East:
        addPiece(state, random, box.maxX + 1, box.minY + yOffset, box.minZ + xOffset, orientation, castle, depth);
        break;
    default:
        break;
    }
}

void childLeft(AssemblyState& state, random::RandomSource& random, const StructureBoundingBox& box,
               StructureDirection orientation, std::int32_t depth, std::int32_t yOffset, std::int32_t zOffset,
               bool castle) {
    switch (orientation) {
    case StructureDirection::North:
    case StructureDirection::South:
        addPiece(state, random, box.minX - 1, box.minY + yOffset, box.minZ + zOffset, StructureDirection::West, castle,
                 depth);
        break;
    case StructureDirection::West:
    case StructureDirection::East:
        addPiece(state, random, box.minX + zOffset, box.minY + yOffset, box.minZ - 1, StructureDirection::North, castle,
                 depth);
        break;
    default:
        break;
    }
}

void childRight(AssemblyState& state, random::RandomSource& random, const StructureBoundingBox& box,
                StructureDirection orientation, std::int32_t depth, std::int32_t yOffset, std::int32_t zOffset,
                bool castle) {
    switch (orientation) {
    case StructureDirection::North:
    case StructureDirection::South:
        addPiece(state, random, box.maxX + 1, box.minY + yOffset, box.minZ + zOffset, StructureDirection::East, castle,
                 depth);
        break;
    case StructureDirection::West:
    case StructureDirection::East:
        addPiece(state, random, box.minX + zOffset, box.minY + yOffset, box.maxZ + 1, StructureDirection::South, castle,
                 depth);
        break;
    default:
        break;
    }
}

void addChildren(AssemblyState& state, std::size_t index, random::RandomSource& random) {
    // The piece lives in a vector the children grow, so its own shape is read out first.
    const StructureBoundingBox box = state.pieces[index].boundingBox;
    const StructureDirection orientation = state.pieces[index].orientation;
    const std::int32_t depth = state.pieces[index].generationDepth;
    switch (state.pieces[index].type) {
    case PieceType::Start:
    case PieceType::BridgeCrossing:
        childForward(state, random, box, orientation, depth, 8, 3, false);
        childLeft(state, random, box, orientation, depth, 3, 8, false);
        childRight(state, random, box, orientation, depth, 3, 8, false);
        break;
    case PieceType::BridgeStraight:
        childForward(state, random, box, orientation, depth, 1, 3, false);
        break;
    case PieceType::RoomCrossing:
        childForward(state, random, box, orientation, depth, 2, 0, false);
        childLeft(state, random, box, orientation, depth, 0, 2, false);
        childRight(state, random, box, orientation, depth, 0, 2, false);
        break;
    case PieceType::StairsRoom:
        childRight(state, random, box, orientation, depth, 6, 2, false);
        break;
    case PieceType::CastleEntrance:
        childForward(state, random, box, orientation, depth, 5, 3, true);
        break;
    case PieceType::CastleSmallCorridor:
        childForward(state, random, box, orientation, depth, 1, 0, true);
        break;
    case PieceType::CastleSmallCrossing:
        childForward(state, random, box, orientation, depth, 1, 0, true);
        childLeft(state, random, box, orientation, depth, 0, 1, true);
        childRight(state, random, box, orientation, depth, 0, 1, true);
        break;
    case PieceType::CastleLeftTurn:
        childLeft(state, random, box, orientation, depth, 0, 1, true);
        break;
    case PieceType::CastleRightTurn:
        childRight(state, random, box, orientation, depth, 0, 1, true);
        break;
    case PieceType::CastleStairs:
        childForward(state, random, box, orientation, depth, 1, 0, true);
        break;
    case PieceType::CastleTBalcony: {
        const std::int32_t zOffset =
            (orientation == StructureDirection::West || orientation == StructureDirection::North) ? 5 : 1;
        childLeft(state, random, box, orientation, depth, 0, zOffset, random.nextInt(8) > 0);
        childRight(state, random, box, orientation, depth, 0, zOffset, random.nextInt(8) > 0);
        break;
    }
    case PieceType::CastleStalkRoom:
        childForward(state, random, box, orientation, depth, 5, 3, true);
        childForward(state, random, box, orientation, depth, 5, 11, true);
        break;
    case PieceType::BridgeEndFiller:
    case PieceType::MonsterThrone:
    default:
        break;
    }
}

} // namespace

std::vector<Piece> assemble(random::RandomSource& random, std::int32_t chunkX, std::int32_t chunkZ) {
    const StructureDirection direction = kHorizontalDirections[static_cast<std::size_t>(
        random.nextInt(static_cast<std::int32_t>(kHorizontalDirections.size())))];
    const StructureBoundingBox startBox = makeBoundingBox(chunkX * 16 + 2, 64, chunkZ * 16 + 2, direction, 19, 10, 19);
    AssemblyState state;
    state.startBox = startBox;
    state.pieces.push_back(
        Piece{.type = PieceType::Start, .generationDepth = 0, .boundingBox = startBox, .orientation = direction});
    addChildren(state, 0, random);
    while (!state.pending.empty()) {
        const std::size_t slot =
            static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(state.pending.size())));
        const std::size_t index = state.pending[slot];
        state.pending.erase(state.pending.begin() + static_cast<std::ptrdiff_t>(slot));
        addChildren(state, index, random);
    }

    StructureBoundingBox bounds = state.pieces.front().boundingBox;
    for (std::size_t index = 1; index < state.pieces.size(); ++index) {
        const StructureBoundingBox& other = state.pieces[index].boundingBox;
        bounds = StructureBoundingBox{std::min(bounds.minX, other.minX), std::min(bounds.minY, other.minY),
                                      std::min(bounds.minZ, other.minZ), std::max(bounds.maxX, other.maxX),
                                      std::max(bounds.maxY, other.maxY), std::max(bounds.maxZ, other.maxZ)};
    }
    const std::int32_t heightSpan = 70 - 48 + 1 - (bounds.maxY - bounds.minY + 1);
    const std::int32_t targetMinY = heightSpan > 1 ? 48 + random.nextInt(heightSpan) : 48;
    const std::int32_t dy = targetMinY - bounds.minY;
    for (Piece& piece : state.pieces) {
        piece.boundingBox = piece.boundingBox.move(StructurePos{0, dy, 0});
    }
    return std::move(state.pieces);
}

bool replaceableByStructures(std::int32_t stateId) {
    if (registry::blocks::isAir(stateId) || registry::blockFluids::hasFluid(stateId)) {
        return true;
    }
    static const std::set<std::string, std::less<>> replaceable{"glow_lichen", "seagrass", "tall_seagrass"};
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && replaceable.contains(block->path());
}

std::int32_t air() { return registry::blocks::air().stateId; }

std::int32_t netherBricks() {
    static const std::int32_t id = plainState("nether_bricks");
    return id;
}

std::int32_t netherBrickFence() {
    static const std::int32_t id = plainState("nether_brick_fence");
    return id;
}

std::int32_t soulSand() {
    static const std::int32_t id = plainState("soul_sand");
    return id;
}

std::int32_t netherWart() {
    static const std::int32_t id = plainState("nether_wart");
    return id;
}

std::int32_t lava() { return registry::blocks::lava().stateId; }

std::int32_t spawner() {
    static const std::int32_t id = plainState("spawner");
    return id;
}

} // namespace basalt::server::world::terrain::netherFortressPieces

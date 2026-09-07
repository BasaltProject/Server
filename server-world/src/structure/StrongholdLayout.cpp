#include <basalt/server/world/structure/StrongholdLayout.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>

namespace basalt::server::world::structure {
namespace {

constexpr std::int32_t kMagicStartY = 64;
constexpr std::int32_t kMaxDepth = 50;
constexpr std::int32_t kLowestY = 10;
constexpr std::int32_t kLeash = 112;
constexpr std::int32_t kBelowSeaLevel = 10;

constexpr std::array<StructureDirection, 4> kHorizontal{StructureDirection::North, StructureDirection::East,
                                                        StructureDirection::South, StructureDirection::West};

[[nodiscard]] StructureDirection horizontal(random::RandomSource& random) {
    return kHorizontal[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
}

/// How often a kind may appear, and from which depth. parity: `STRONGHOLD_PIECE_WEIGHTS`.
struct PieceWeight {
    StrongholdPieceKind kind = StrongholdPieceKind::Straight;
    std::int32_t weight = 0;
    std::int32_t maxPlaceCount = 0;
    std::int32_t minDepth = 0;
    std::int32_t placeCount = 0;

    [[nodiscard]] bool doPlace(std::int32_t depth) const {
        return (maxPlaceCount == 0 || placeCount < maxPlaceCount) && depth > minDepth;
    }

    [[nodiscard]] bool isValid() const { return maxPlaceCount == 0 || placeCount < maxPlaceCount; }
};

[[nodiscard]] std::vector<PieceWeight> defaultWeights() {
    return {PieceWeight{.kind = StrongholdPieceKind::Straight, .weight = 40, .maxPlaceCount = 0},
            PieceWeight{.kind = StrongholdPieceKind::PrisonHall, .weight = 5, .maxPlaceCount = 5},
            PieceWeight{.kind = StrongholdPieceKind::LeftTurn, .weight = 20, .maxPlaceCount = 0},
            PieceWeight{.kind = StrongholdPieceKind::RightTurn, .weight = 20, .maxPlaceCount = 0},
            PieceWeight{.kind = StrongholdPieceKind::RoomCrossing, .weight = 10, .maxPlaceCount = 6},
            PieceWeight{.kind = StrongholdPieceKind::StraightStairsDown, .weight = 5, .maxPlaceCount = 5},
            PieceWeight{.kind = StrongholdPieceKind::StairsDown, .weight = 5, .maxPlaceCount = 5},
            PieceWeight{.kind = StrongholdPieceKind::FiveCrossing, .weight = 5, .maxPlaceCount = 4},
            PieceWeight{.kind = StrongholdPieceKind::ChestCorridor, .weight = 5, .maxPlaceCount = 4},
            PieceWeight{.kind = StrongholdPieceKind::Library, .weight = 10, .maxPlaceCount = 2, .minDepth = 4},
            PieceWeight{.kind = StrongholdPieceKind::PortalRoom, .weight = 20, .maxPlaceCount = 1, .minDepth = 5}};
}

/// One attempt at the maze: the pieces placed so far, plus the weights they have used up.
class Walk {
public:
    explicit Walk(random::RandomSource& random) : random_(&random), weights_(defaultWeights()) {}

    [[nodiscard]] std::vector<StrongholdPiece>& pieces() noexcept { return pieces_; }

    [[nodiscard]] std::vector<std::size_t>& pending() noexcept { return pending_; }

    [[nodiscard]] bool hasPortalRoom() const noexcept { return hasPortalRoom_; }

    void addChildren(std::size_t index);

private:
    /// parity: `generateSmallDoorChildForward`, which leaves through the face the piece points at.
    void forward(std::size_t index, std::int32_t xOff, std::int32_t yOff);
    /// parity: `generateSmallDoorChildLeft`. North and south both leave through the west face.
    void left(std::size_t index, std::int32_t yOff, std::int32_t zOff);
    /// parity: `generateSmallDoorChildRight`, the mirror of the one above.
    void right(std::size_t index, std::int32_t yOff, std::int32_t zOff);
    /// parity: `generateAndAddPiece`, with its 112-block leash and its depth ceiling.
    void grow(std::int32_t footX, std::int32_t footY, std::int32_t footZ, StructureDirection direction,
              std::int32_t depth);
    /// parity: `generatePieceFromSmallDoor`. Five attempts at the weighted table, each of which gives
    /// up the moment its chosen kind is refused, and then a filler corridor.
    [[nodiscard]] std::optional<StrongholdPiece> pieceFromSmallDoor(std::int32_t footX, std::int32_t footY,
                                                                    std::int32_t footZ, StructureDirection direction,
                                                                    std::int32_t depth);
    /// parity: `updatePieceWeight`, which also refuses to go on once no capped kind is left.
    [[nodiscard]] bool updateWeights();
    /// parity: each kind's `createPiece`: build the box, refuse it below y 10 or on a collision.
    [[nodiscard]] std::optional<StrongholdPiece> create(StrongholdPieceKind kind, std::int32_t footX,
                                                        std::int32_t footY, std::int32_t footZ,
                                                        StructureDirection direction, std::int32_t depth);
    [[nodiscard]] std::optional<StrongholdPiece> simple(StrongholdPieceKind kind, StructureBoundingBox candidate,
                                                        StructureDirection direction, std::int32_t depth);
    /// parity: `randomSmallDoor`: two of the five draws are the plain opening.
    [[nodiscard]] StrongholdDoor randomDoor();
    /// parity: `FillerCorridor.findPieceBox`. A filler is only laid where something is already in the
    /// way at the same height, and it is shortened until it just touches it.
    [[nodiscard]] std::optional<StructureBoundingBox> fillerBox(std::int32_t footX, std::int32_t footY,
                                                                std::int32_t footZ, StructureDirection direction);
    [[nodiscard]] std::optional<StructureBoundingBox> collision(const StructureBoundingBox& box) const;

    random::RandomSource* random_;
    std::vector<StrongholdPiece> pieces_;
    std::vector<std::size_t> pending_;
    bool hasPortalRoom_ = false;
    std::vector<PieceWeight> weights_;
    std::optional<StrongholdPieceKind> previous_;
    std::optional<StrongholdPieceKind> imposed_;
    std::int32_t totalWeight_ = 0;
};

void Walk::addChildren(std::size_t index) {
    const StrongholdPiece piece = pieces_[index];
    switch (piece.kind) {
    case StrongholdPieceKind::Start:
    case StrongholdPieceKind::StairsDown:
        if (piece.isSource) {
            imposed_ = StrongholdPieceKind::FiveCrossing;
        }
        forward(index, 1, 1);
        return;
    case StrongholdPieceKind::Straight:
        forward(index, 1, 1);
        if (piece.leftChild) {
            left(index, 1, 2);
        }
        if (piece.rightChild) {
            right(index, 1, 2);
        }
        return;
    case StrongholdPieceKind::PrisonHall:
    case StrongholdPieceKind::ChestCorridor:
    case StrongholdPieceKind::StraightStairsDown:
        forward(index, 1, 1);
        return;
    case StrongholdPieceKind::LeftTurn:
        if (piece.orientation != StructureDirection::North && piece.orientation != StructureDirection::East) {
            right(index, 1, 1);
        } else {
            left(index, 1, 1);
        }
        return;
    case StrongholdPieceKind::RightTurn:
        if (piece.orientation != StructureDirection::North && piece.orientation != StructureDirection::East) {
            left(index, 1, 1);
        } else {
            right(index, 1, 1);
        }
        return;
    case StrongholdPieceKind::RoomCrossing:
        forward(index, 4, 1);
        left(index, 1, 4);
        right(index, 1, 4);
        return;
    case StrongholdPieceKind::FiveCrossing: {
        std::int32_t lowOffset = 3;
        std::int32_t highOffset = 5;
        if (piece.orientation == StructureDirection::West || piece.orientation == StructureDirection::North) {
            lowOffset = 8 - lowOffset;
            highOffset = 8 - highOffset;
        }
        forward(index, 5, 1);
        if (piece.leftLow) {
            left(index, lowOffset, 1);
        }
        if (piece.leftHigh) {
            left(index, highOffset, 7);
        }
        if (piece.rightLow) {
            right(index, lowOffset, 1);
        }
        if (piece.rightHigh) {
            right(index, highOffset, 7);
        }
        return;
    }
    case StrongholdPieceKind::PortalRoom:
        hasPortalRoom_ = true;
        return;
    case StrongholdPieceKind::Library:
    case StrongholdPieceKind::FillerCorridor:
        return;
    }
}

void Walk::forward(std::size_t index, std::int32_t xOff, std::int32_t yOff) {
    const StrongholdPiece& piece = pieces_[index];
    const StructureBoundingBox box = piece.box;
    switch (piece.orientation) {
    case StructureDirection::North:
        grow(box.minX + xOff, box.minY + yOff, box.minZ - 1, StructureDirection::North, piece.depth);
        return;
    case StructureDirection::South:
        grow(box.minX + xOff, box.minY + yOff, box.maxZ + 1, StructureDirection::South, piece.depth);
        return;
    case StructureDirection::West:
        grow(box.minX - 1, box.minY + yOff, box.minZ + xOff, StructureDirection::West, piece.depth);
        return;
    case StructureDirection::East:
        grow(box.maxX + 1, box.minY + yOff, box.minZ + xOff, StructureDirection::East, piece.depth);
        return;
    default:
        return;
    }
}

void Walk::left(std::size_t index, std::int32_t yOff, std::int32_t zOff) {
    const StrongholdPiece& piece = pieces_[index];
    const StructureBoundingBox box = piece.box;
    switch (piece.orientation) {
    case StructureDirection::North:
    case StructureDirection::South:
        grow(box.minX - 1, box.minY + yOff, box.minZ + zOff, StructureDirection::West, piece.depth);
        return;
    case StructureDirection::West:
    case StructureDirection::East:
        grow(box.minX + zOff, box.minY + yOff, box.minZ - 1, StructureDirection::North, piece.depth);
        return;
    default:
        return;
    }
}

void Walk::right(std::size_t index, std::int32_t yOff, std::int32_t zOff) {
    const StrongholdPiece& piece = pieces_[index];
    const StructureBoundingBox box = piece.box;
    switch (piece.orientation) {
    case StructureDirection::North:
    case StructureDirection::South:
        grow(box.maxX + 1, box.minY + yOff, box.minZ + zOff, StructureDirection::East, piece.depth);
        return;
    case StructureDirection::West:
    case StructureDirection::East:
        grow(box.minX + zOff, box.minY + yOff, box.maxZ + 1, StructureDirection::South, piece.depth);
        return;
    default:
        return;
    }
}

void Walk::grow(std::int32_t footX, std::int32_t footY, std::int32_t footZ, StructureDirection direction,
                std::int32_t depth) {
    if (depth > kMaxDepth) {
        return;
    }
    const StructureBoundingBox start = pieces_.front().box;
    if (std::abs(footX - start.minX) > kLeash || std::abs(footZ - start.minZ) > kLeash) {
        return;
    }
    std::optional<StrongholdPiece> piece = pieceFromSmallDoor(footX, footY, footZ, direction, depth + 1);
    if (!piece.has_value()) {
        return;
    }
    pieces_.push_back(std::move(*piece));
    pending_.push_back(pieces_.size() - 1);
}

bool Walk::updateWeights() {
    bool any = false;
    totalWeight_ = 0;
    for (const PieceWeight& entry : weights_) {
        if (entry.maxPlaceCount > 0 && entry.placeCount < entry.maxPlaceCount) {
            any = true;
        }
        totalWeight_ += entry.weight;
    }
    return any;
}

std::optional<StrongholdPiece> Walk::pieceFromSmallDoor(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                        StructureDirection direction, std::int32_t depth) {
    if (!updateWeights()) {
        return std::nullopt;
    }
    if (imposed_.has_value()) {
        const StrongholdPieceKind kind = *imposed_;
        imposed_.reset();
        std::optional<StrongholdPiece> piece = create(kind, footX, footY, footZ, direction, depth);
        if (piece.has_value()) {
            return piece;
        }
    }
    for (std::int32_t attempts = 0; attempts < 5; ++attempts) {
        std::int32_t selection = random_->nextInt(totalWeight_);
        for (std::size_t index = 0; index < weights_.size(); ++index) {
            selection -= weights_[index].weight;
            if (selection >= 0) {
                continue;
            }
            if (!weights_[index].doPlace(depth) || previous_ == weights_[index].kind) {
                break;
            }
            // parity: a kind that does not fit does not end the attempt. The running total stays
            // negative, so every remaining kind is tried in table order until one fits.
            std::optional<StrongholdPiece> piece = create(weights_[index].kind, footX, footY, footZ, direction, depth);
            if (!piece.has_value()) {
                continue;
            }
            ++weights_[index].placeCount;
            previous_ = weights_[index].kind;
            if (!weights_[index].isValid()) {
                weights_.erase(weights_.begin() + static_cast<std::ptrdiff_t>(index));
            }
            return piece;
        }
    }
    const std::optional<StructureBoundingBox> box = fillerBox(footX, footY, footZ, direction);
    if (!box.has_value() || box->minY <= 1) {
        return std::nullopt;
    }
    const bool alongZ = direction == StructureDirection::North || direction == StructureDirection::South;
    return StrongholdPiece{.kind = StrongholdPieceKind::FillerCorridor,
                           .box = *box,
                           .orientation = direction,
                           .depth = depth,
                           .entryDoor = StrongholdDoor::Opening,
                           .steps = alongZ ? box->maxZ - box->minZ + 1 : box->maxX - box->minX + 1};
}

StrongholdDoor Walk::randomDoor() {
    switch (random_->nextInt(5)) {
    case 2:
        return StrongholdDoor::WoodDoor;
    case 3:
        return StrongholdDoor::Grates;
    case 4:
        return StrongholdDoor::IronDoor;
    default:
        return StrongholdDoor::Opening;
    }
}

std::optional<StrongholdPiece> Walk::simple(StrongholdPieceKind kind, StructureBoundingBox candidate,
                                            StructureDirection direction, std::int32_t depth) {
    if (candidate.minY <= kLowestY || collision(candidate).has_value()) {
        return std::nullopt;
    }
    return StrongholdPiece{
        .kind = kind, .box = candidate, .orientation = direction, .depth = depth, .entryDoor = randomDoor()};
}

std::optional<StrongholdPiece> Walk::create(StrongholdPieceKind kind, std::int32_t footX, std::int32_t footY,
                                            std::int32_t footZ, StructureDirection direction, std::int32_t depth) {
    const auto box = [&](std::int32_t offX, std::int32_t offY, std::int32_t width, std::int32_t height,
                         std::int32_t pieceDepth) {
        return strongholdLayout::orientBox(footX, footY, footZ, offX, offY, 0, width, height, pieceDepth, direction);
    };
    const auto free = [&](const StructureBoundingBox& candidate) {
        return candidate.minY > kLowestY && !collision(candidate).has_value();
    };

    switch (kind) {
    case StrongholdPieceKind::Straight: {
        const StructureBoundingBox candidate = box(-1, -1, 5, 5, 7);
        if (!free(candidate)) {
            return std::nullopt;
        }
        const StrongholdDoor door = randomDoor();
        return StrongholdPiece{.kind = kind,
                               .box = candidate,
                               .orientation = direction,
                               .depth = depth,
                               .entryDoor = door,
                               .leftChild = random_->nextInt(2) == 0,
                               .rightChild = random_->nextInt(2) == 0};
    }
    case StrongholdPieceKind::PrisonHall:
        return simple(kind, box(-1, -1, 9, 5, 11), direction, depth);
    case StrongholdPieceKind::LeftTurn:
    case StrongholdPieceKind::RightTurn:
        return simple(kind, box(-1, -1, 5, 5, 5), direction, depth);
    case StrongholdPieceKind::RoomCrossing: {
        const StructureBoundingBox candidate = box(-4, -1, 11, 7, 11);
        if (!free(candidate)) {
            return std::nullopt;
        }
        const StrongholdDoor door = randomDoor();
        return StrongholdPiece{.kind = kind,
                               .box = candidate,
                               .orientation = direction,
                               .depth = depth,
                               .entryDoor = door,
                               .roomType = random_->nextInt(5)};
    }
    case StrongholdPieceKind::StraightStairsDown:
        return simple(kind, box(-1, -7, 5, 11, 8), direction, depth);
    case StrongholdPieceKind::StairsDown:
        return simple(kind, box(-1, -7, 5, 11, 5), direction, depth);
    case StrongholdPieceKind::FiveCrossing: {
        const StructureBoundingBox candidate = box(-4, -3, 10, 9, 11);
        if (!free(candidate)) {
            return std::nullopt;
        }
        const StrongholdDoor door = randomDoor();
        return StrongholdPiece{.kind = kind,
                               .box = candidate,
                               .orientation = direction,
                               .depth = depth,
                               .entryDoor = door,
                               .leftLow = random_->nextBoolean(),
                               .leftHigh = random_->nextBoolean(),
                               .rightLow = random_->nextBoolean(),
                               .rightHigh = random_->nextInt(3) > 0};
    }
    case StrongholdPieceKind::ChestCorridor:
        return simple(kind, box(-1, -1, 5, 5, 7), direction, depth);
    case StrongholdPieceKind::Library: {
        // parity: the tall library is tried first and the short one is the fallback, so a library
        // that fits only at six blocks still counts against the cap.
        StructureBoundingBox candidate = box(-4, -1, 14, 11, 15);
        if (!free(candidate)) {
            candidate = box(-4, -1, 14, 6, 15);
            if (!free(candidate)) {
                return std::nullopt;
            }
        }
        const StrongholdDoor door = randomDoor();
        return StrongholdPiece{.kind = kind,
                               .box = candidate,
                               .orientation = direction,
                               .depth = depth,
                               .entryDoor = door,
                               .tall = candidate.maxY - candidate.minY + 1 > 6};
    }
    case StrongholdPieceKind::PortalRoom: {
        // parity: the portal room is the only kind whose constructor draws nothing.
        const StructureBoundingBox candidate = box(-4, -1, 11, 8, 16);
        if (!free(candidate)) {
            return std::nullopt;
        }
        return StrongholdPiece{.kind = kind,
                               .box = candidate,
                               .orientation = direction,
                               .depth = depth,
                               .entryDoor = StrongholdDoor::Opening};
    }
    default:
        return std::nullopt;
    }
}

std::optional<StructureBoundingBox> Walk::fillerBox(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                    StructureDirection direction) {
    const StructureBoundingBox full = strongholdLayout::orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 4, direction);
    const std::optional<StructureBoundingBox> hit = collision(full);
    if (!hit.has_value() || hit->minY != full.minY) {
        return std::nullopt;
    }
    for (std::int32_t length = 2; length >= 1; --length) {
        const StructureBoundingBox shorter =
            strongholdLayout::orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, length, direction);
        if (!hit->intersects(shorter)) {
            return strongholdLayout::orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, length + 1, direction);
        }
    }
    return std::nullopt;
}

std::optional<StructureBoundingBox> Walk::collision(const StructureBoundingBox& box) const {
    for (const StrongholdPiece& piece : pieces_) {
        if (piece.box.intersects(box)) {
            return piece.box;
        }
    }
    return std::nullopt;
}

/// parity: `StructurePiecesBuilder.moveBelowSeaLevel`, which drops the whole maze so that its top
/// sits ten blocks under the sea, or lower still when there is room to spare.
void moveBelowSeaLevel(std::vector<StrongholdPiece>& pieces, random::RandomSource& random, std::int32_t seaLevel,
                       std::int32_t minY) {
    if (pieces.empty()) {
        return;
    }
    std::int32_t minYSeen = std::numeric_limits<std::int32_t>::max();
    std::int32_t maxYSeen = std::numeric_limits<std::int32_t>::min();
    for (const StrongholdPiece& piece : pieces) {
        minYSeen = std::min(minYSeen, piece.box.minY);
        maxYSeen = std::max(maxYSeen, piece.box.maxY);
    }
    const std::int32_t highest = seaLevel - kBelowSeaLevel;
    std::int32_t top = (maxYSeen - minYSeen + 1) + minY + 1;
    if (top < highest) {
        top += random.nextInt(highest - top);
    }
    const std::int32_t dy = top - maxYSeen;
    for (StrongholdPiece& piece : pieces) {
        piece.box.minY += dy;
        piece.box.maxY += dy;
    }
}

} // namespace

StructureBoundingBox strongholdLayout::makeBox(std::int32_t x, std::int32_t y, std::int32_t z,
                                               StructureDirection direction, std::int32_t width, std::int32_t height,
                                               std::int32_t depth) {
    if (direction == StructureDirection::North || direction == StructureDirection::South) {
        return StructureBoundingBox{x, y, z, x + width - 1, y + height - 1, z + depth - 1};
    }
    return StructureBoundingBox{x, y, z, x + depth - 1, y + height - 1, z + width - 1};
}

StructureBoundingBox strongholdLayout::orientBox(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                 std::int32_t offX, std::int32_t offY, std::int32_t offZ,
                                                 std::int32_t width, std::int32_t height, std::int32_t depth,
                                                 StructureDirection direction) {
    switch (direction) {
    case StructureDirection::North:
        return StructureBoundingBox{
            footX + offX, footY + offY, footZ - depth + 1 + offZ, footX + width - 1 + offX, footY + height - 1 + offY,
            footZ + offZ};
    case StructureDirection::West:
        return StructureBoundingBox{
            footX - depth + 1 + offZ, footY + offY, footZ + offX, footX + offZ, footY + height - 1 + offY,
            footZ + width - 1 + offX};
    case StructureDirection::East:
        return StructureBoundingBox{footX + offZ,
                                    footY + offY,
                                    footZ + offX,
                                    footX + depth - 1 + offZ,
                                    footY + height - 1 + offY,
                                    footZ + width - 1 + offX};
    default:
        return StructureBoundingBox{footX + offX,
                                    footY + offY,
                                    footZ + offZ,
                                    footX + width - 1 + offX,
                                    footY + height - 1 + offY,
                                    footZ + depth - 1 + offZ};
    }
}

std::vector<StrongholdPiece> strongholdLayout::build(random::RandomSource& random, const Reseed& reseed,
                                                     std::int32_t chunkX, std::int32_t chunkZ, std::int32_t seaLevel,
                                                     std::int32_t minY) {
    std::int32_t attempt = 0;
    while (true) {
        reseed(attempt++);
        Walk walk{random};
        const StructureDirection facing = horizontal(random);
        walk.pieces().push_back(
            StrongholdPiece{.kind = StrongholdPieceKind::Start,
                            .box = makeBox(chunkX * 16 + 2, kMagicStartY, chunkZ * 16 + 2, facing, 5, 11, 5),
                            .orientation = facing,
                            .depth = 0,
                            .entryDoor = StrongholdDoor::Opening,
                            .isSource = true});
        walk.addChildren(0);
        while (!walk.pending().empty()) {
            const auto index =
                static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(walk.pending().size())));
            const std::size_t next = walk.pending()[index];
            walk.pending().erase(walk.pending().begin() + static_cast<std::ptrdiff_t>(index));
            walk.addChildren(next);
        }
        moveBelowSeaLevel(walk.pieces(), random, seaLevel, minY);
        if (!walk.pieces().empty() && walk.hasPortalRoom()) {
            return std::move(walk.pieces());
        }
    }
}

} // namespace basalt::server::world::structure

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/structure/MineshaftLayout.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>

namespace basalt::server::world::structure {
namespace {

constexpr std::int32_t kMagicStartY = 50;
constexpr std::int32_t kMaxDepth = 8;
constexpr std::int32_t kMaxReach = 80;
constexpr std::int32_t kSeaLevelMargin = 10;
constexpr std::int32_t kUpperFloorOffset = 4;

[[nodiscard]] bool axisIsZ(StructureDirection direction) {
    return direction == StructureDirection::North || direction == StructureDirection::South;
}

[[nodiscard]] StructureBoundingBox moved(StructureBoundingBox box, std::int32_t x, std::int32_t y, std::int32_t z) {
    return box.move(StructurePos{x, y, z});
}

/// parity: `StructurePiecesBuilder` as `MineshaftPieces` uses it. Holds the pieces added so far so a
/// candidate box can be rejected for overlapping one, which is what keeps a shaft from folding back
/// into itself.
class Builder {
public:
    explicit Builder(random::RandomSource& random) : random_(&random) {}

    [[nodiscard]] std::vector<MineshaftPiece>& pieces() noexcept { return pieces_; }

    void addRoom(std::int32_t west, std::int32_t north);
    /// Returns the vertical offset applied, which is what sets the structure's start y.
    std::int32_t moveBelowSeaLevel(std::int32_t seaLevel, std::int32_t minY);

    /// The mesa variant is lifted to the surface instead of buried. Same return.
    std::int32_t moveToSurface(std::int32_t seaLevel, const mineshaftLayout::SurfaceHeightAt& surfaceHeightAt);

    void offsetPiecesVertically(std::int32_t dy);

private:
    [[nodiscard]] bool collides(const StructureBoundingBox& box) const {
        return std::any_of(pieces_.begin(), pieces_.end(),
                           [&box](const MineshaftPiece& piece) { return piece.box.intersects(box); });
    }

    [[nodiscard]] std::optional<StructureBoundingBox> bounds() const;

    /// parity: `MineshaftPieces.generateAndAddPiece`, including its 80-block leash to the room.
    std::optional<std::size_t> generateAndAddPiece(std::size_t startIndex, std::int32_t footX, std::int32_t footY,
                                                   std::int32_t footZ, StructureDirection direction,
                                                   std::int32_t depth);

    [[nodiscard]] std::optional<MineshaftPiece> createRandomPiece(std::int32_t footX, std::int32_t footY,
                                                                  std::int32_t footZ, StructureDirection direction,
                                                                  std::int32_t genDepth);

    /// parity: the `MineShaftCorridor` constructor, whose rolls come after its box is accepted.
    [[nodiscard]] MineshaftPiece corridor(StructureBoundingBox box, StructureDirection direction,
                                          std::int32_t genDepth);

    [[nodiscard]] std::optional<StructureBoundingBox> findCorridor(std::int32_t footX, std::int32_t footY,
                                                                   std::int32_t footZ, StructureDirection direction);
    [[nodiscard]] std::optional<StructureBoundingBox> findCrossing(std::int32_t footX, std::int32_t footY,
                                                                   std::int32_t footZ, StructureDirection direction);
    [[nodiscard]] std::optional<StructureBoundingBox> findStairs(std::int32_t footX, std::int32_t footY,
                                                                 std::int32_t footZ, StructureDirection direction);

    void addChildren(std::size_t startIndex, std::size_t pieceIndex);
    void addCorridorChildren(std::size_t startIndex, std::size_t pieceIndex);
    void addCrossingChildren(std::size_t startIndex, std::size_t pieceIndex);
    void addStairsChildren(std::size_t startIndex, std::size_t pieceIndex);
    /// Four passes around the room, each stepping along a wall and opening a passage or not.
    [[nodiscard]] std::vector<StructureBoundingBox> addRoomChildren(std::size_t roomIndex);

    random::RandomSource* random_;
    std::vector<MineshaftPiece> pieces_;
};

void Builder::addRoom(std::int32_t west, std::int32_t north) {
    const StructureBoundingBox box{west,
                                   kMagicStartY,
                                   north,
                                   west + 7 + random_->nextInt(6),
                                   kMagicStartY + 4 + random_->nextInt(6),
                                   north + 7 + random_->nextInt(6)};
    pieces_.push_back(MineshaftPiece{.kind = MineshaftPiece::Kind::Room, .box = box, .genDepth = 0});
    std::vector<StructureBoundingBox> entrances = addRoomChildren(0);
    pieces_[0].entrances = std::move(entrances);
}

std::optional<StructureBoundingBox> Builder::bounds() const {
    if (pieces_.empty()) {
        return std::nullopt;
    }
    StructureBoundingBox box = pieces_.front().box;
    for (const MineshaftPiece& piece : pieces_) {
        box = StructureBoundingBox{std::min(box.minX, piece.box.minX), std::min(box.minY, piece.box.minY),
                                   std::min(box.minZ, piece.box.minZ), std::max(box.maxX, piece.box.maxX),
                                   std::max(box.maxY, piece.box.maxY), std::max(box.maxZ, piece.box.maxZ)};
    }
    return box;
}

std::int32_t Builder::moveBelowSeaLevel(std::int32_t seaLevel, std::int32_t minY) {
    const std::optional<StructureBoundingBox> box = bounds();
    if (!box.has_value()) {
        return 0;
    }
    const std::int32_t maxY = seaLevel - kSeaLevelMargin;
    // getYSpan is inclusive, so the span is maxY - minY + 1 and not the difference.
    std::int32_t top = (box->maxY - box->minY + 1) + minY + 1;
    if (top < maxY) {
        top += random_->nextInt(maxY - top);
    }
    const std::int32_t dy = top - box->maxY;
    offsetPiecesVertically(dy);
    return dy;
}

std::int32_t Builder::moveToSurface(std::int32_t seaLevel, const mineshaftLayout::SurfaceHeightAt& surfaceHeightAt) {
    const std::optional<StructureBoundingBox> box = bounds();
    if (!box.has_value()) {
        return 0;
    }
    // parity: `getCenter` rounds up on an even span, so the centre is not (min + max) / 2.
    const std::int32_t centreX = box->minX + ((box->maxX - box->minX + 1) / 2);
    const std::int32_t centreY = box->minY + ((box->maxY - box->minY + 1) / 2);
    const std::int32_t centreZ = box->minZ + ((box->maxZ - box->minZ + 1) / 2);
    const std::int32_t surfaceHeight = surfaceHeightAt(centreX, centreZ);
    // parity: a shaft under water stays at sea level; above it, `randomBetweenInclusive` picks a
    // height between sea level and the surface, and that draw only happens in that case.
    const std::int32_t target =
        surfaceHeight <= seaLevel ? seaLevel : seaLevel + random_->nextInt(surfaceHeight - seaLevel + 1);
    const std::int32_t dy = target - centreY;
    offsetPiecesVertically(dy);
    return dy;
}

void Builder::offsetPiecesVertically(std::int32_t dy) {
    for (MineshaftPiece& piece : pieces_) {
        piece.box.minY += dy;
        piece.box.maxY += dy;
        for (StructureBoundingBox& entrance : piece.entrances) {
            entrance.minY += dy;
            entrance.maxY += dy;
        }
    }
}

std::optional<std::size_t> Builder::generateAndAddPiece(std::size_t startIndex, std::int32_t footX, std::int32_t footY,
                                                        std::int32_t footZ, StructureDirection direction,
                                                        std::int32_t depth) {
    if (depth > kMaxDepth) {
        return std::nullopt;
    }
    const StructureBoundingBox startBox = pieces_[startIndex].box;
    if (std::abs(footX - startBox.minX) > kMaxReach || std::abs(footZ - startBox.minZ) > kMaxReach) {
        return std::nullopt;
    }
    std::optional<MineshaftPiece> piece = createRandomPiece(footX, footY, footZ, direction, depth + 1);
    if (!piece.has_value()) {
        return std::nullopt;
    }
    pieces_.push_back(std::move(*piece));
    const std::size_t index = pieces_.size() - 1;
    addChildren(startIndex, index);
    return index;
}

std::optional<MineshaftPiece> Builder::createRandomPiece(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                         StructureDirection direction, std::int32_t genDepth) {
    const std::int32_t selection = random_->nextInt(100);
    if (selection >= 80) {
        const std::optional<StructureBoundingBox> box = findCrossing(footX, footY, footZ, direction);
        if (!box.has_value()) {
            return std::nullopt;
        }
        return MineshaftPiece{.kind = MineshaftPiece::Kind::Crossing,
                              .box = *box,
                              .orientation = direction,
                              .genDepth = genDepth,
                              .twoFloored = box->maxY - box->minY + 1 > 3};
    }
    if (selection >= 70) {
        const std::optional<StructureBoundingBox> box = findStairs(footX, footY, footZ, direction);
        if (!box.has_value()) {
            return std::nullopt;
        }
        return MineshaftPiece{
            .kind = MineshaftPiece::Kind::Stairs, .box = *box, .orientation = direction, .genDepth = genDepth};
    }
    const std::optional<StructureBoundingBox> box = findCorridor(footX, footY, footZ, direction);
    if (!box.has_value()) {
        return std::nullopt;
    }
    return corridor(*box, direction, genDepth);
}

MineshaftPiece Builder::corridor(StructureBoundingBox box, StructureDirection direction, std::int32_t genDepth) {
    const bool hasRails = random_->nextInt(3) == 0;
    const bool spider = !hasRails && random_->nextInt(23) == 0;
    const std::int32_t span = axisIsZ(direction) ? box.maxZ - box.minZ + 1 : box.maxX - box.minX + 1;
    return MineshaftPiece{.kind = MineshaftPiece::Kind::Corridor,
                          .box = box,
                          .orientation = direction,
                          .genDepth = genDepth,
                          .hasRails = hasRails,
                          .spiderCorridor = spider,
                          .sections = span / 5};
}

std::optional<StructureBoundingBox> Builder::findCorridor(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                          StructureDirection direction) {
    std::int32_t length = random_->nextInt(3) + 2;
    while (length > 0) {
        const std::int32_t blocks = length * 5;
        StructureBoundingBox box;
        switch (direction) {
        case StructureDirection::South:
            box = StructureBoundingBox{0, 0, 0, 2, 2, blocks - 1};
            break;
        case StructureDirection::West:
            box = StructureBoundingBox{-(blocks - 1), 0, 0, 0, 2, 2};
            break;
        case StructureDirection::East:
            box = StructureBoundingBox{0, 0, 0, blocks - 1, 2, 2};
            break;
        default:
            box = StructureBoundingBox{0, 0, -(blocks - 1), 2, 2, 0};
            break;
        }
        box = moved(box, footX, footY, footZ);
        if (!collides(box)) {
            return box;
        }
        --length;
    }
    return std::nullopt;
}

std::optional<StructureBoundingBox> Builder::findCrossing(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                          StructureDirection direction) {
    const std::int32_t height = random_->nextInt(4) == 0 ? 6 : 2;
    StructureBoundingBox box;
    switch (direction) {
    case StructureDirection::South:
        box = StructureBoundingBox{-1, 0, 0, 3, height, 4};
        break;
    case StructureDirection::West:
        box = StructureBoundingBox{-4, 0, -1, 0, height, 3};
        break;
    case StructureDirection::East:
        box = StructureBoundingBox{0, 0, -1, 4, height, 3};
        break;
    default:
        box = StructureBoundingBox{-1, 0, -4, 3, height, 0};
        break;
    }
    box = moved(box, footX, footY, footZ);
    return collides(box) ? std::nullopt : std::optional{box};
}

std::optional<StructureBoundingBox> Builder::findStairs(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                                        StructureDirection direction) {
    StructureBoundingBox box;
    switch (direction) {
    case StructureDirection::South:
        box = StructureBoundingBox{0, -5, 0, 2, 2, 8};
        break;
    case StructureDirection::West:
        box = StructureBoundingBox{-8, -5, 0, 0, 2, 2};
        break;
    case StructureDirection::East:
        box = StructureBoundingBox{0, -5, 0, 8, 2, 2};
        break;
    default:
        box = StructureBoundingBox{0, -5, -8, 2, 2, 0};
        break;
    }
    box = moved(box, footX, footY, footZ);
    return collides(box) ? std::nullopt : std::optional{box};
}

void Builder::addChildren(std::size_t startIndex, std::size_t pieceIndex) {
    switch (pieces_[pieceIndex].kind) {
    case MineshaftPiece::Kind::Corridor:
        addCorridorChildren(startIndex, pieceIndex);
        return;
    case MineshaftPiece::Kind::Crossing:
        addCrossingChildren(startIndex, pieceIndex);
        return;
    case MineshaftPiece::Kind::Stairs:
        addStairsChildren(startIndex, pieceIndex);
        return;
    case MineshaftPiece::Kind::Room:
        static_cast<void>(addRoomChildren(pieceIndex));
        return;
    }
}

void Builder::addCorridorChildren(std::size_t startIndex, std::size_t pieceIndex) {
    const std::int32_t depth = pieces_[pieceIndex].genDepth;
    const StructureBoundingBox box = pieces_[pieceIndex].box;
    const std::optional<StructureDirection> orientation = pieces_[pieceIndex].orientation;
    const std::int32_t end = random_->nextInt(4);

    const auto ahead = [&](std::int32_t x, std::int32_t z, StructureDirection direction) {
        static_cast<void>(generateAndAddPiece(startIndex, x, box.minY - 1 + random_->nextInt(3), z, direction, depth));
    };
    if (orientation == StructureDirection::South) {
        if (end <= 1) {
            ahead(box.minX, box.maxZ + 1, StructureDirection::South);
        } else if (end == 2) {
            ahead(box.minX - 1, box.maxZ - 3, StructureDirection::West);
        } else {
            ahead(box.maxX + 1, box.maxZ - 3, StructureDirection::East);
        }
    } else if (orientation == StructureDirection::West) {
        if (end <= 1) {
            ahead(box.minX - 1, box.minZ, StructureDirection::West);
        } else if (end == 2) {
            ahead(box.minX, box.minZ - 1, StructureDirection::North);
        } else {
            ahead(box.minX, box.maxZ + 1, StructureDirection::South);
        }
    } else if (orientation == StructureDirection::East) {
        if (end <= 1) {
            ahead(box.maxX + 1, box.minZ, StructureDirection::East);
        } else if (end == 2) {
            ahead(box.maxX - 3, box.minZ - 1, StructureDirection::North);
        } else {
            ahead(box.maxX - 3, box.maxZ + 1, StructureDirection::South);
        }
    } else {
        if (end <= 1) {
            ahead(box.minX, box.minZ - 1, StructureDirection::North);
        } else if (end == 2) {
            ahead(box.minX - 1, box.minZ, StructureDirection::West);
        } else {
            ahead(box.maxX + 1, box.minZ, StructureDirection::East);
        }
    }
    if (depth >= kMaxDepth) {
        return;
    }
    // Side passages every five blocks along the corridor, one roll each.
    if (!orientation.has_value() || !axisIsZ(*orientation)) {
        for (std::int32_t x = box.minX + 3; x + 3 <= box.maxX; x += 5) {
            const std::int32_t roll = random_->nextInt(5);
            if (roll == 0) {
                static_cast<void>(
                    generateAndAddPiece(startIndex, x, box.minY, box.minZ - 1, StructureDirection::North, depth + 1));
            } else if (roll == 1) {
                static_cast<void>(
                    generateAndAddPiece(startIndex, x, box.minY, box.maxZ + 1, StructureDirection::South, depth + 1));
            }
        }
    } else {
        for (std::int32_t z = box.minZ + 3; z + 3 <= box.maxZ; z += 5) {
            const std::int32_t roll = random_->nextInt(5);
            if (roll == 0) {
                static_cast<void>(
                    generateAndAddPiece(startIndex, box.minX - 1, box.minY, z, StructureDirection::West, depth + 1));
            } else if (roll == 1) {
                static_cast<void>(
                    generateAndAddPiece(startIndex, box.maxX + 1, box.minY, z, StructureDirection::East, depth + 1));
            }
        }
    }
}

void Builder::addCrossingChildren(std::size_t startIndex, std::size_t pieceIndex) {
    const std::int32_t depth = pieces_[pieceIndex].genDepth;
    const StructureBoundingBox box = pieces_[pieceIndex].box;
    const std::optional<StructureDirection> orientation = pieces_[pieceIndex].orientation;
    const bool twoFloored = pieces_[pieceIndex].twoFloored;
    const auto add = [&](std::int32_t x, std::int32_t y, std::int32_t z, StructureDirection direction) {
        static_cast<void>(generateAndAddPiece(startIndex, x, y, z, direction, depth));
    };
    if (orientation == StructureDirection::South) {
        add(box.minX + 1, box.minY, box.maxZ + 1, StructureDirection::South);
        add(box.minX - 1, box.minY, box.minZ + 1, StructureDirection::West);
        add(box.maxX + 1, box.minY, box.minZ + 1, StructureDirection::East);
    } else if (orientation == StructureDirection::West) {
        add(box.minX + 1, box.minY, box.minZ - 1, StructureDirection::North);
        add(box.minX + 1, box.minY, box.maxZ + 1, StructureDirection::South);
        add(box.minX - 1, box.minY, box.minZ + 1, StructureDirection::West);
    } else if (orientation == StructureDirection::East) {
        add(box.minX + 1, box.minY, box.minZ - 1, StructureDirection::North);
        add(box.minX + 1, box.minY, box.maxZ + 1, StructureDirection::South);
        add(box.maxX + 1, box.minY, box.minZ + 1, StructureDirection::East);
    } else {
        add(box.minX + 1, box.minY, box.minZ - 1, StructureDirection::North);
        add(box.minX - 1, box.minY, box.minZ + 1, StructureDirection::West);
        add(box.maxX + 1, box.minY, box.minZ + 1, StructureDirection::East);
    }
    if (!twoFloored) {
        return;
    }
    const std::int32_t upper = box.minY + kUpperFloorOffset;
    if (random_->nextBoolean()) {
        add(box.minX + 1, upper, box.minZ - 1, StructureDirection::North);
    }
    if (random_->nextBoolean()) {
        add(box.minX - 1, upper, box.minZ + 1, StructureDirection::West);
    }
    if (random_->nextBoolean()) {
        add(box.maxX + 1, upper, box.minZ + 1, StructureDirection::East);
    }
    if (random_->nextBoolean()) {
        add(box.minX + 1, upper, box.maxZ + 1, StructureDirection::South);
    }
}

void Builder::addStairsChildren(std::size_t startIndex, std::size_t pieceIndex) {
    const std::int32_t depth = pieces_[pieceIndex].genDepth;
    const StructureBoundingBox box = pieces_[pieceIndex].box;
    const std::optional<StructureDirection> orientation = pieces_[pieceIndex].orientation;
    const auto add = [&](std::int32_t x, std::int32_t y, std::int32_t z, StructureDirection direction) {
        static_cast<void>(generateAndAddPiece(startIndex, x, y, z, direction, depth));
    };
    if (orientation == StructureDirection::South) {
        add(box.minX, box.minY, box.maxZ + 1, StructureDirection::South);
    } else if (orientation == StructureDirection::West) {
        add(box.minX - 1, box.minY, box.minZ, StructureDirection::West);
    } else if (orientation == StructureDirection::East) {
        add(box.maxX + 1, box.minY, box.minZ, StructureDirection::East);
    } else {
        add(box.minX, box.minY, box.minZ - 1, StructureDirection::North);
    }
}

std::vector<StructureBoundingBox> Builder::addRoomChildren(std::size_t roomIndex) {
    std::vector<StructureBoundingBox> entrances;
    const std::int32_t depth = pieces_[roomIndex].genDepth;
    const StructureBoundingBox box = pieces_[roomIndex].box;
    const std::int32_t xSpan = box.maxX - box.minX + 1;
    const std::int32_t zSpan = box.maxZ - box.minZ + 1;
    const std::int32_t headroom = std::max(box.maxY - box.minY + 1 - 3 - 1, 1);

    const auto wall = [&](std::int32_t span,
                          const std::function<std::optional<std::size_t>(std::int32_t, std::int32_t)>& place,
                          const std::function<StructureBoundingBox(const StructureBoundingBox&)>& mouth) {
        std::int32_t pos = 0;
        while (pos < span) {
            pos += random_->nextInt(span);
            if (pos + 3 > span) {
                break;
            }
            const std::optional<std::size_t> child = place(pos, box.minY + random_->nextInt(headroom) + 1);
            if (child.has_value()) {
                entrances.push_back(mouth(pieces_[*child].box));
            }
            pos += 4;
        }
    };

    wall(
        xSpan,
        [&](std::int32_t pos, std::int32_t y) {
            return generateAndAddPiece(roomIndex, box.minX + pos, y, box.minZ - 1, StructureDirection::North, depth);
        },
        [&](const StructureBoundingBox& child) {
            return StructureBoundingBox{child.minX, child.minY, box.minZ, child.maxX, child.maxY, box.minZ + 1};
        });
    wall(
        xSpan,
        [&](std::int32_t pos, std::int32_t y) {
            return generateAndAddPiece(roomIndex, box.minX + pos, y, box.maxZ + 1, StructureDirection::South, depth);
        },
        [&](const StructureBoundingBox& child) {
            return StructureBoundingBox{child.minX, child.minY, box.maxZ - 1, child.maxX, child.maxY, box.maxZ};
        });
    wall(
        zSpan,
        [&](std::int32_t pos, std::int32_t y) {
            return generateAndAddPiece(roomIndex, box.minX - 1, y, box.minZ + pos, StructureDirection::West, depth);
        },
        [&](const StructureBoundingBox& child) {
            return StructureBoundingBox{box.minX, child.minY, child.minZ, box.minX + 1, child.maxY, child.maxZ};
        });
    wall(
        zSpan,
        [&](std::int32_t pos, std::int32_t y) {
            return generateAndAddPiece(roomIndex, box.maxX + 1, y, box.minZ + pos, StructureDirection::East, depth);
        },
        [&](const StructureBoundingBox& child) {
            return StructureBoundingBox{box.maxX - 1, child.minY, child.minZ, box.maxX, child.maxY, child.maxZ};
        });
    return entrances;
}

/// parity: `findGenerationPoint` builds its start position at y 50 before any offset.
constexpr std::int32_t kStartY = 50;

} // namespace

mineshaftLayout::Assembly mineshaftLayout::assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                                    MineshaftType type, std::int32_t seaLevel, std::int32_t minY,
                                                    const SurfaceHeightAt& surfaceHeightAt) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(random, seed, chunkX, chunkZ);
    // parity: `MineshaftStructure.findGenerationPoint` throws this first draw away, and every roll of
    // the whole shaft hangs off what comes after it.
    static_cast<void>(random.nextDouble());
    Builder builder{random};
    builder.addRoom(chunkX * 16 + 2, chunkZ * 16 + 2);
    if (type == MineshaftType::Mesa) {
        if (!surfaceHeightAt) {
            return Assembly{.pieces = std::move(builder.pieces()), .stubY = kStartY};
        }
        const std::int32_t dy = builder.moveToSurface(seaLevel, surfaceHeightAt);
        return Assembly{.pieces = std::move(builder.pieces()), .stubY = kStartY + dy};
    }
    const std::int32_t dy = builder.moveBelowSeaLevel(seaLevel, minY);
    return Assembly{.pieces = std::move(builder.pieces()), .stubY = kStartY + dy};
}

} // namespace basalt::server::world::structure

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/StrongholdLibrary.hpp>
#include <basalt/server/world/structure/StrongholdRooms.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace basalt::server::world::terrain {
namespace {

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kBeardRadius = 12;
/// A maze reaches 112 blocks out from the chunk it starts in, plus the last room's own size.
constexpr std::int32_t kReachChunks = 9;

[[nodiscard]] std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) << 32) | (static_cast<std::int64_t>(chunkZ) & 0xFFFFFFFFLL);
}

[[nodiscard]] structure::StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ,
                                                        std::int32_t margin = 0) {
    return structure::StructureBoundingBox{chunkX * 16 - margin,    kOverworldHeight.minY(),
                                           chunkZ * 16 - margin,    chunkX * 16 + 15 + margin,
                                           kOverworldHeight.maxY(), chunkZ * 16 + 15 + margin};
}

[[nodiscard]] structure::StructureBoundingBox wholeBox(const std::vector<structure::StrongholdPiece>& pieces) {
    structure::StructureBoundingBox box = pieces.front().box;
    for (const structure::StrongholdPiece& piece : pieces) {
        box = structure::StructureBoundingBox{std::min(box.minX, piece.box.minX), std::min(box.minY, piece.box.minY),
                                              std::min(box.minZ, piece.box.minZ), std::max(box.maxX, piece.box.maxX),
                                              std::max(box.maxY, piece.box.maxY), std::max(box.maxZ, piece.box.maxZ)};
    }
    return box;
}

void draw(Chunk& chunk, const structure::StrongholdPiece& piece, const structure::StructureBoundingBox& clip,
          random::RandomSource& random) {
    const structure::StructurePieceFill fill{piece.box, piece.orientation, chunk, clip};
    const structure::StrongholdBrush brush{fill, chunk, random};
    switch (piece.kind) {
    case structure::StrongholdPieceKind::Start:
    case structure::StrongholdPieceKind::StairsDown:
        structure::strongholdCorridors::stairsDown(brush, piece);
        return;
    case structure::StrongholdPieceKind::Straight:
        structure::strongholdCorridors::straight(brush, piece);
        return;
    case structure::StrongholdPieceKind::StraightStairsDown:
        structure::strongholdCorridors::straightStairsDown(brush, piece);
        return;
    case structure::StrongholdPieceKind::LeftTurn:
    case structure::StrongholdPieceKind::RightTurn:
        structure::strongholdCorridors::turn(brush, piece);
        return;
    case structure::StrongholdPieceKind::ChestCorridor:
        structure::strongholdCorridors::chestCorridor(brush, piece);
        return;
    case structure::StrongholdPieceKind::FillerCorridor:
        structure::strongholdCorridors::fillerCorridor(brush, piece);
        return;
    case structure::StrongholdPieceKind::PrisonHall:
        structure::strongholdRooms::prisonHall(brush, piece);
        return;
    case structure::StrongholdPieceKind::RoomCrossing:
        structure::strongholdRooms::roomCrossing(brush, piece);
        return;
    case structure::StrongholdPieceKind::FiveCrossing:
        structure::strongholdRooms::fiveCrossing(brush, piece);
        return;
    case structure::StrongholdPieceKind::Library:
        structure::strongholdLibrary::place(brush, piece);
        return;
    case structure::StrongholdPieceKind::PortalRoom:
        structure::strongholdPortalRoom::place(brush);
        return;
    }
}

} // namespace

StrongholdStructureGenerator::StrongholdStructureGenerator(structure::ConcentricRingsPlacement rings, std::int64_t seed,
                                                           NoiseBiomeAt noiseBiomeAt,
                                                           std::set<std::string, std::less<>> overworldBiomes,
                                                           std::int32_t seaLevel)
    : rings_(rings), seed_(seed), noiseBiomeAt_(std::move(noiseBiomeAt)), overworldBiomes_(std::move(overworldBiomes)),
      seaLevel_(seaLevel) {}

void StrongholdStructureGenerator::ensureRings() const {
    const std::scoped_lock lock{mutex_};
    if (ringsBuilt_) {
        return;
    }
    ringPositions_ = rings_.ringPositions(seed_);
    for (const auto& [chunkX, chunkZ] : ringPositions_) {
        ringChunks_.insert(key(chunkX, chunkZ));
    }
    ringsBuilt_ = true;
}

bool StrongholdStructureGenerator::validBiome(std::int32_t chunkX, std::int32_t chunkZ) const {
    return overworldBiomes_.contains(noiseBiomeAt_((chunkX * 16) >> 2, 0, (chunkZ * 16) >> 2));
}

std::vector<std::pair<std::int32_t, std::int32_t>>
StrongholdStructureGenerator::nearbyStarts(std::int32_t chunkX, std::int32_t chunkZ) const {
    ensureRings();
    if (ringChunks_.empty()) {
        return {};
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> found;
    for (std::int32_t dx = -kReachChunks; dx <= kReachChunks; ++dx) {
        for (std::int32_t dz = -kReachChunks; dz <= kReachChunks; ++dz) {
            const std::int32_t startX = chunkX + dx;
            const std::int32_t startZ = chunkZ + dz;
            if (!ringChunks_.contains(key(startX, startZ))) {
                continue;
            }
            if (!validBiome(startX, startZ)) {
                continue;
            }
            found.emplace_back(startX, startZ);
        }
    }
    return found;
}

const std::vector<structure::StrongholdPiece>&
StrongholdStructureGenerator::layoutFor(std::int32_t startX, std::int32_t startZ, std::int64_t seed) {
    const std::int64_t chunkKey = key(startX, startZ);
    {
        const std::scoped_lock lock{mutex_};
        if (const auto found = layouts_.find(chunkKey); found != layouts_.end()) {
            return found->second;
        }
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    std::vector<structure::StrongholdPiece> built = structure::strongholdLayout::build(
        random, [&](std::int32_t attempt) { random::setLargeFeatureSeed(random, seed + attempt, startX, startZ); },
        startX, startZ, seaLevel_, kOverworldHeight.minY());
    const std::scoped_lock lock{mutex_};
    if (const auto found = layouts_.find(chunkKey); found != layouts_.end()) {
        return found->second;
    }
    return layouts_.emplace(chunkKey, std::move(built)).first->second;
}

void StrongholdStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const structure::StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ());
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    bool seeded = false;
    for (const auto& [startX, startZ] : nearbyStarts(chunk.chunkX(), chunk.chunkZ())) {
        const std::vector<structure::StrongholdPiece>& pieces = layoutFor(startX, startZ, seed);
        std::vector<structure::StructureBoundingBox> here;
        for (const structure::StrongholdPiece& piece : pieces) {
            if (piece.box.intersects(clip)) {
                here.push_back(piece.box);
            }
        }
        if (here.empty()) {
            continue;
        }
        if (!seeded) {
            const std::int64_t decorationSeed =
                random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
            random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);
            seeded = true;
        }
        for (const structure::StrongholdPiece& piece : pieces) {
            if (piece.box.intersects(clip)) {
                draw(chunk, piece, clip, random);
            }
        }
        chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                               .chunkX = startX,
                                                               .chunkZ = startZ,
                                                               .fullBounds = wholeBox(pieces),
                                                               .pieces = std::move(here)});
    }
}

std::optional<structure::StructureBoundingBox>
StrongholdStructureGenerator::placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::vector<structure::StrongholdPiece>& pieces = layoutFor(chunkX, chunkZ, seed);
    if (pieces.empty()) {
        return std::nullopt;
    }
    return wholeBox(pieces);
}

void StrongholdStructureGenerator::placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    const structure::StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ());
    const std::vector<structure::StrongholdPiece>& pieces = layoutFor(chunkX, chunkZ, seed);
    bool anythingHere = false;
    for (const structure::StrongholdPiece& piece : pieces) {
        if (piece.box.intersects(clip)) {
            anythingHere = true;
            break;
        }
    }
    if (!anythingHere) {
        return;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);
    for (const structure::StrongholdPiece& piece : pieces) {
        if (piece.box.intersects(clip)) {
            draw(chunk, piece, clip, random);
        }
    }
    // parity: `PlaceCommand.placeStructure` writes the pieces but registers no start.
}

std::optional<std::pair<std::int32_t, std::int32_t>> StrongholdStructureGenerator::locate(std::int32_t fromX,
                                                                                          std::int32_t fromZ) const {
    ensureRings();
    std::optional<std::pair<std::int32_t, std::int32_t>> best;
    double bestDistance = std::numeric_limits<double>::max();
    for (const auto& [chunkX, chunkZ] : ringPositions_) {
        const double dx = static_cast<double>(chunkX * 16 + 8) - fromX;
        const double dz = static_cast<double>(chunkZ * 16 + 8) - fromZ;
        const double distance = dx * dx + dz * dz;
        if (best.has_value() && distance >= bestDistance) {
            continue;
        }
        best = std::pair{chunkX, chunkZ};
        bestDistance = distance;
    }
    return best;
}

void StrongholdStructureGenerator::addBeard(StructureBeard& beard, std::int32_t chunkX, std::int32_t chunkZ,
                                            std::int64_t seed) {
    const structure::StructureBoundingBox nearClip = chunkClip(chunkX, chunkZ, kBeardRadius);
    std::vector<BeardifierRigid>& rigids = beard.pieces;
    for (const auto& [startX, startZ] : nearbyStarts(chunkX, chunkZ)) {
        for (const structure::StrongholdPiece& piece : layoutFor(startX, startZ, seed)) {
            if (!piece.box.intersects(nearClip)) {
                continue;
            }
            rigids.push_back(
                BeardifierRigid{.box = piece.box, .groundLevelDelta = 0, .adjustment = TerrainAdjustment::Bury});
        }
    }
}

} // namespace basalt::server::world::terrain

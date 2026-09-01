#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/structure/MonumentDoubleRooms.hpp>
#include <basalt/server/world/structure/MonumentShell.hpp>
#include <basalt/server/world/structure/MonumentSingleRooms.hpp>
#include <basalt/server/world/structure/MonumentWings.hpp>
#include <basalt/server/world/terrain/OceanMonumentStructureGenerator.hpp>

#include <array>
#include <cstdlib>
#include <map>
#include <optional>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

using structure::StructureDirection;

constexpr std::int32_t kSurfaceStructuresStep = 4;
/// The monument is 58 blocks wide and starts 29 west of its chunk, so it reaches two chunks out.
constexpr std::int32_t kReachChunks = 2;
constexpr std::int32_t kRadius = 29;
/// parity: `makeBoundingBox(west, 39, north, ...)`; the roof lands on y 61 whatever the sea bed does.
constexpr std::int32_t kBaseY = 39;

constexpr std::array<StructureDirection, 4> kHorizontal{StructureDirection::North, StructureDirection::East,
                                                        StructureDirection::South, StructureDirection::West};

struct RoomSize {
    std::int32_t width = 1;
    std::int32_t height = 1;
    std::int32_t depth = 1;
};

/// Each room kind's size in lattice cells, as its piece constructor names it.
[[nodiscard]] std::optional<RoomSize> sizeOf(structure::MonumentRoomKind kind) {
    switch (kind) {
    case structure::MonumentRoomKind::Entry:
    case structure::MonumentRoomKind::Simple:
    case structure::MonumentRoomKind::SimpleTop:
        return RoomSize{1, 1, 1};
    case structure::MonumentRoomKind::Core:
        return RoomSize{2, 2, 2};
    case structure::MonumentRoomKind::DoubleX:
        return RoomSize{2, 1, 1};
    case structure::MonumentRoomKind::DoubleXY:
        return RoomSize{2, 2, 1};
    case structure::MonumentRoomKind::DoubleY:
        return RoomSize{1, 2, 1};
    case structure::MonumentRoomKind::DoubleYZ:
        return RoomSize{1, 2, 2};
    case structure::MonumentRoomKind::DoubleZ:
        return RoomSize{1, 1, 2};
    default:
        return std::nullopt;
    }
}

[[nodiscard]] structure::StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ) {
    return structure::StructureBoundingBox{chunkX * 16,      kOverworldHeight.minY(), chunkZ * 16,
                                           chunkX * 16 + 15, kOverworldHeight.maxY(), chunkZ * 16 + 15};
}

void draw(const structure::MonumentRoomPiece& piece, const structure::MonumentBrush& brush,
          random::RandomSource& random) {
    const structure::MonumentRoom* room = piece.room;
    switch (piece.kind) {
    case structure::MonumentRoomKind::Entry:
        if (room != nullptr) {
            structure::monumentSingleRooms::entry(brush, *room);
        }
        return;
    case structure::MonumentRoomKind::Core:
        structure::monumentSingleRooms::core(brush);
        return;
    case structure::MonumentRoomKind::Simple:
        if (room != nullptr) {
            structure::monumentSingleRooms::simple(brush, *room, piece.design, random);
        }
        return;
    case structure::MonumentRoomKind::SimpleTop:
        if (room != nullptr) {
            structure::monumentSingleRooms::simpleTop(brush, *room, random);
        }
        return;
    case structure::MonumentRoomKind::DoubleX:
        if (room != nullptr) {
            structure::monumentDoubleRooms::doubleX(brush, *room);
        }
        return;
    case structure::MonumentRoomKind::DoubleXY:
        if (room != nullptr) {
            structure::monumentDoubleRooms::doubleXy(brush, *room);
        }
        return;
    case structure::MonumentRoomKind::DoubleY:
        if (room != nullptr) {
            structure::monumentDoubleRooms::doubleY(brush, *room);
        }
        return;
    case structure::MonumentRoomKind::DoubleYZ:
        if (room != nullptr) {
            structure::monumentDoubleRooms::doubleYz(brush, *room);
        }
        return;
    case structure::MonumentRoomKind::DoubleZ:
        if (room != nullptr) {
            structure::monumentDoubleRooms::doubleZ(brush, *room);
        }
        return;
    case structure::MonumentRoomKind::LeftWing:
    case structure::MonumentRoomKind::RightWing:
        structure::monumentWings::wing(brush, piece.design);
        return;
    case structure::MonumentRoomKind::Penthouse:
        structure::monumentWings::penthouse(brush);
        return;
    }
}

/// parity: `OceanMonumentPiece.makeBoundingBox`. A room's box is its lattice cell scaled by eight and
/// four, laid out along the monument's facing and then moved by the building's own offset. The wings
/// and the penthouse are not lattice cells: their corners are named outright.
[[nodiscard]] std::optional<structure::StructureBoundingBox> boxOf(const structure::MonumentRoomPiece& piece,
                                                                   StructureDirection orientation,
                                                                   const structure::StructurePieceFill& shell,
                                                                   structure::StructurePos offset) {
    const auto corners = [&shell](std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                                  std::int32_t z1) {
        return structure::StructureBoundingBox::fromCorners(
            structure::StructurePos{shell.worldX(x0, z0), shell.worldY(y0), shell.worldZ(x0, z0)},
            structure::StructurePos{shell.worldX(x1, z1), shell.worldY(y1), shell.worldZ(x1, z1)});
    };
    if (piece.kind == structure::MonumentRoomKind::LeftWing) {
        return corners(1, 1, 1, 23, 8, 21);
    }
    if (piece.kind == structure::MonumentRoomKind::RightWing) {
        return corners(34, 1, 1, 56, 8, 21);
    }
    if (piece.kind == structure::MonumentRoomKind::Penthouse) {
        return corners(22, 13, 22, 35, 17, 35);
    }
    if (piece.room == nullptr) {
        return std::nullopt;
    }
    const std::optional<RoomSize> size = sizeOf(piece.kind);
    if (!size.has_value()) {
        return std::nullopt;
    }
    const std::int32_t roomX = piece.room->index() % 5;
    const std::int32_t roomZ = piece.room->index() / 5 % 5;
    const std::int32_t roomY = piece.room->index() / 25;
    const std::int32_t width = size->width * 8;
    const std::int32_t height = size->height * 4;
    const std::int32_t depth = size->depth * 8;
    const bool alongZ = orientation == StructureDirection::North || orientation == StructureDirection::South;
    const structure::StructureBoundingBox base =
        alongZ ? structure::StructureBoundingBox{0, 0, 0, width - 1, height - 1, depth - 1}
               : structure::StructureBoundingBox{0, 0, 0, depth - 1, height - 1, width - 1};
    structure::StructurePos step;
    switch (orientation) {
    case StructureDirection::North:
        step = structure::StructurePos{roomX * 8, roomY * 4, -(roomZ + size->depth) * 8 + 1};
        break;
    case StructureDirection::South:
        step = structure::StructurePos{roomX * 8, roomY * 4, roomZ * 8};
        break;
    case StructureDirection::West:
        step = structure::StructurePos{-(roomZ + size->depth) * 8 + 1, roomY * 4, roomX * 8};
        break;
    default:
        step = structure::StructurePos{roomZ * 8, roomY * 4, roomX * 8};
        break;
    }
    return base.move(step).move(offset);
}

} // namespace

OceanMonumentStructureGenerator::OceanMonumentStructureGenerator(NoiseBiomeAt noiseBiomeAt,
                                                                 std::set<std::string, std::less<>> monumentBiomes,
                                                                 std::set<std::string, std::less<>> surroundingBiomes,
                                                                 OceanFloorAt oceanFloorAt, std::int32_t seaLevel)
    : noiseBiomeAt_(std::move(noiseBiomeAt)), monumentBiomes_(std::move(monumentBiomes)),
      surroundingBiomes_(std::move(surroundingBiomes)), oceanFloorAt_(std::move(oceanFloorAt)), seaLevel_(seaLevel) {}

const structure::StructurePlacementGrid& OceanMonumentStructureGenerator::grid() {
    static const structure::StructurePlacementGrid built{32, 5, 10387313, structure::SpreadType::Triangular};
    return built;
}

bool OceanMonumentStructureGenerator::surroundingIsOcean(std::int32_t chunkX, std::int32_t chunkZ) const {
    const std::int32_t x = chunkX * 16 + 9;
    const std::int32_t z = chunkZ * 16 + 9;
    for (std::int32_t quartX = (x - kRadius) >> 2; quartX <= ((x + kRadius) >> 2); ++quartX) {
        for (std::int32_t quartY = (seaLevel_ - kRadius) >> 2; quartY <= ((seaLevel_ + kRadius) >> 2); ++quartY) {
            for (std::int32_t quartZ = (z - kRadius) >> 2; quartZ <= ((z + kRadius) >> 2); ++quartZ) {
                if (!surroundingBiomes_.contains(noiseBiomeAt_(quartX, quartY, quartZ))) {
                    return false;
                }
            }
        }
    }
    return true;
}

void OceanMonumentStructureGenerator::placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX,
                                                std::int32_t chunkZ) {
    if (!grid().isStructureChunk(seed, chunkX, chunkZ)) {
        return;
    }
    if (!surroundingIsOcean(chunkX, chunkZ)) {
        return;
    }

    // parity: `onTopOfChunkCenter` over `OCEAN_FLOOR_WG`, which only decides where the biome is
    // sampled: the monument itself never moves off y 39.
    const std::int32_t centreX = chunkX * 16 + 8;
    const std::int32_t centreZ = chunkZ * 16 + 8;
    const std::int32_t groundY = oceanFloorAt_(centreX, centreZ) - 1;
    if (!monumentBiomes_.contains(noiseBiomeAt_(centreX >> 2, groundY >> 2, centreZ >> 2))) {
        return;
    }

    random::RandomSource startRandom = random::RandomSource::worldgen(random::RandomSource::legacy(0));
    random::setLargeFeatureSeed(startRandom, seed, chunkX, chunkZ);
    const StructureDirection orientation =
        kHorizontal[static_cast<std::size_t>(startRandom.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
    structure::MonumentRoomGraph graph = structure::generateMonumentRoomGraph(startRandom);
    const std::vector<structure::MonumentRoomPiece> rooms = structure::fitMonumentRooms(graph, startRandom);

    const std::int32_t west = chunkX * 16 - 29;
    const std::int32_t north = chunkZ * 16 - 29;
    const structure::StructureBoundingBox shellBox{west, kBaseY, north, west + 57, kBaseY + 22, north + 57};
    const structure::StructureBoundingBox clip = chunkClip(chunk.chunkX(), chunk.chunkZ());
    if (!shellBox.intersects(clip)) {
        return;
    }

    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kStructureIndex, kSurfaceStructuresStep);

    std::vector<structure::StructureBoundingBox> here;
    const structure::StructurePieceFill shellFill{shellBox, orientation, chunk, clip};
    structure::monumentShell::place(structure::MonumentBrush{shellFill, seaLevel_, clip}, seaLevel_);
    here.push_back(shellBox);

    // parity: the children are placed in the order `MonumentBuilding` built them, and a room only
    // draws where its own box reaches this chunk, so a room's draws happen once per chunk it touches.
    // The offset is the building's own (9, 0, 22) turned by the monument's facing.
    const structure::StructurePos offset{shellFill.worldX(9, 22), shellFill.worldY(0), shellFill.worldZ(9, 22)};
    for (const structure::MonumentRoomPiece& piece : rooms) {
        const std::optional<structure::StructureBoundingBox> box = boxOf(piece, orientation, shellFill, offset);
        if (!box.has_value() || !box->intersects(clip)) {
            continue;
        }
        here.push_back(*box);
        const structure::StructurePieceFill fill{*box, orientation, chunk, clip};
        draw(piece, structure::MonumentBrush{fill, seaLevel_, clip}, random);
    }
    chunk.structures().add(structure::StructureStartRecord{.structure = std::string{kName},
                                                           .chunkX = chunkX,
                                                           .chunkZ = chunkZ,
                                                           .fullBounds = shellBox,
                                                           .pieces = std::move(here)});
}

void OceanMonumentStructureGenerator::generateInto(Chunk& chunk, std::int64_t seed) {
    const std::int32_t spacing = grid().spacing();
    const auto floorDiv = [](std::int32_t value, std::int32_t divisor) {
        const std::int32_t quotient = value / divisor;
        return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
    };
    const std::int32_t cellX = floorDiv(chunk.chunkX(), spacing);
    const std::int32_t cellZ = floorDiv(chunk.chunkZ(), spacing);
    for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            const auto [candidateX, candidateZ] =
                grid().candidateChunk(seed, (cellX + offsetX) * spacing, (cellZ + offsetZ) * spacing);
            if (std::abs(candidateX - chunk.chunkX()) > kReachChunks ||
                std::abs(candidateZ - chunk.chunkZ()) > kReachChunks) {
                continue;
            }
            placeFrom(chunk, seed, candidateX, candidateZ);
        }
    }
}

} // namespace basalt::server::world::terrain

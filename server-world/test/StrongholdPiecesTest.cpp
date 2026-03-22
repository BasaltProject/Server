#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StrongholdLibrary.hpp>
#include <basalt/server/world/structure/StrongholdRooms.hpp>

#include <doctest.h>

#include <cstdint>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::structure;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] Chunk stoneChunk() {
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 60; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 0; y <= 60; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

void draw(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    switch (piece.kind) {
    case StrongholdPieceKind::Start:
    case StrongholdPieceKind::StairsDown:
        strongholdCorridors::stairsDown(brush, piece);
        return;
    case StrongholdPieceKind::Straight:
        strongholdCorridors::straight(brush, piece);
        return;
    case StrongholdPieceKind::StraightStairsDown:
        strongholdCorridors::straightStairsDown(brush, piece);
        return;
    case StrongholdPieceKind::LeftTurn:
    case StrongholdPieceKind::RightTurn:
        strongholdCorridors::turn(brush, piece);
        return;
    case StrongholdPieceKind::ChestCorridor:
        strongholdCorridors::chestCorridor(brush, piece);
        return;
    case StrongholdPieceKind::FillerCorridor:
        strongholdCorridors::fillerCorridor(brush, piece);
        return;
    case StrongholdPieceKind::PrisonHall:
        strongholdRooms::prisonHall(brush, piece);
        return;
    case StrongholdPieceKind::RoomCrossing:
        strongholdRooms::roomCrossing(brush, piece);
        return;
    case StrongholdPieceKind::FiveCrossing:
        strongholdRooms::fiveCrossing(brush, piece);
        return;
    case StrongholdPieceKind::Library:
        strongholdLibrary::place(brush, piece);
        return;
    case StrongholdPieceKind::PortalRoom:
        strongholdPortalRoom::place(brush);
        return;
    }
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
    std::int32_t draws = 0;
};

struct Options {
    std::int32_t roomType = 0;
    bool tall = false;
    std::int32_t steps = 0;
    bool leftChild = false;
    bool rightChild = false;
    std::int64_t seed = 777;
};

[[nodiscard]] Run run(StrongholdPieceKind kind, StructureDirection orientation, StrongholdDoor door,
                      const Options& options = {}) {
    Chunk chunk = stoneChunk();
    const StructureBoundingBox box{0, 6, 0, 15, 40, 15};
    const StructureBoundingBox clip{0, 0, 0, 15, 60, 15};
    const StructurePieceFill fill{box, orientation, chunk, clip};
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(options.seed));
    const StrongholdBrush brush{fill, chunk, random};
    const StrongholdPiece piece{.kind = kind,
                                .box = box,
                                .orientation = orientation,
                                .depth = 1,
                                .entryDoor = door,
                                .leftLow = true,
                                .rightHigh = true,
                                .leftChild = options.leftChild,
                                .rightChild = options.rightChild,
                                .roomType = options.roomType,
                                .tall = options.tall,
                                .steps = options.steps};
    draw(brush, piece);
    Run result;
    const auto [checksum, written, blockEntities] = fold(chunk);
    result.written = written;
    result.blockEntities = blockEntities;
    result.deferred = chunk.takeGeneratedBlockWrites().size();
    result.checksum = checksum;
    result.after = random.nextInt();
    return result;
}

/// The corridor again over eight seeds, folded into one run: one roll landing in a shifted chance's
/// window is enough, and one seed rarely does.
[[nodiscard]] Run runTorches() {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    std::uint32_t draws = 0;
    for (std::int64_t seed = 1; seed <= 8; ++seed) {
        Chunk chunk = stoneChunk();
        const StructureBoundingBox box{0, 6, 0, 15, 40, 15};
        const StructureBoundingBox clip{0, 0, 0, 15, 60, 15};
        const StructurePieceFill fill{box, StructureDirection::East, chunk, clip};
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed));
        const StrongholdBrush brush{fill, chunk, random};
        strongholdCorridors::straight(brush, StrongholdPiece{.kind = StrongholdPieceKind::Straight,
                                                             .box = box,
                                                             .orientation = StructureDirection::East,
                                                             .depth = 1,
                                                             .entryDoor = StrongholdDoor::Grates,
                                                             .leftChild = true,
                                                             .rightChild = true});
        const auto [chunkChecksum, written, blockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        draws = draws * 31U + static_cast<std::uint32_t>(random.nextInt(1 << 20));
    }
    result.draws = static_cast<std::int32_t>(draws);
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t deferred,
           std::int64_t checksum, std::int32_t after) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.deferred == deferred);
    CHECK(result.checksum == checksum);
    CHECK(result.after == after);
}

} // namespace

TEST_CASE("every stronghold room draws what the frozen kotlin draws") {
    SUBCASE("stairsDown") {
        check(run(StrongholdPieceKind::StairsDown, StructureDirection::North, StrongholdDoor::Opening), 275, 0, 0,
              -7822406708046913399LL, -1824631828);
    }

    SUBCASE("straightStairs") {
        check(run(StrongholdPieceKind::StraightStairsDown, StructureDirection::South, StrongholdDoor::WoodDoor), 440, 0,
              0, 1228774412350454967LL, 1367554549);
    }

    SUBCASE("straight") {
        check(run(StrongholdPieceKind::Straight, StructureDirection::East, StrongholdDoor::Grates,
                  Options{.leftChild = true, .rightChild = true}),
              175, 0, 0, -1486676689707968034LL, -1225253890);
    }

    SUBCASE("straightTorches") {
        const Run result = runTorches();
        CHECK(result.written == 1400);
        CHECK(result.blockEntities == 0);
        CHECK(result.deferred == 0);
        CHECK(result.draws == -2002832741);
        CHECK(result.checksum == 3228296681694291465LL);
    }

    SUBCASE("leftTurnNorth") {
        check(run(StrongholdPieceKind::LeftTurn, StructureDirection::North, StrongholdDoor::Opening), 125, 0, 0,
              -4814328370327003494LL, -1563540256);
    }

    SUBCASE("leftTurnWest") {
        check(run(StrongholdPieceKind::LeftTurn, StructureDirection::West, StrongholdDoor::Opening), 125, 0, 0,
              -7648640418882953589LL, -1563540256);
    }

    SUBCASE("rightTurnNorth") {
        check(run(StrongholdPieceKind::RightTurn, StructureDirection::North, StrongholdDoor::Opening), 125, 0, 0,
              -6977579985591403689LL, -1563540256);
    }

    SUBCASE("chestCorridor") {
        check(run(StrongholdPieceKind::ChestCorridor, StructureDirection::North, StrongholdDoor::IronDoor), 175, 1, 0,
              6182468844020567033LL, 1400005628);
    }

    SUBCASE("filler") {
        check(run(StrongholdPieceKind::FillerCorridor, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.steps = 4}),
              100, 0, 0, -92997234752584491LL, -1257443035);
    }

    SUBCASE("prisonHall") {
        check(run(StrongholdPieceKind::PrisonHall, StructureDirection::North, StrongholdDoor::Opening), 495, 0, 0,
              6939441259923071338LL, -1421634423);
    }

    SUBCASE("roomCrossing0") {
        check(run(StrongholdPieceKind::RoomCrossing, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.roomType = 0}),
              847, 0, 0, -7968822720024932015LL, 618992086);
    }

    SUBCASE("roomCrossing1") {
        check(run(StrongholdPieceKind::RoomCrossing, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.roomType = 1}),
              847, 0, 0, 2903098822033606670LL, 618992086);
    }

    SUBCASE("roomCrossing2") {
        check(run(StrongholdPieceKind::RoomCrossing, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.roomType = 2}),
              847, 1, 0, -6289456503443951974LL, -1758423617);
    }

    SUBCASE("roomCrossing3") {
        check(run(StrongholdPieceKind::RoomCrossing, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.roomType = 3}),
              847, 0, 0, -9173343759359332903LL, 618992086);
    }

    SUBCASE("roomCrossing4") {
        check(run(StrongholdPieceKind::RoomCrossing, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.roomType = 4}),
              847, 0, 0, -9173343759359332903LL, 618992086);
    }

    SUBCASE("fiveCrossing") {
        check(run(StrongholdPieceKind::FiveCrossing, StructureDirection::North, StrongholdDoor::Opening), 990, 0, 0,
              -6110292646545857232LL, -590291558);
    }

    SUBCASE("libraryShort") {
        check(run(StrongholdPieceKind::Library, StructureDirection::North, StrongholdDoor::Opening), 1260, 1, 0,
              -5962210426891443650LL, 2131326114);
    }

    SUBCASE("libraryTall") {
        check(run(StrongholdPieceKind::Library, StructureDirection::North, StrongholdDoor::Opening,
                  Options{.tall = true}),
              2310, 2, 0, -8354924805334925365LL, -1435806088);
    }

    SUBCASE("portalRoom") {
        check(run(StrongholdPieceKind::PortalRoom, StructureDirection::North, StrongholdDoor::Grates), 1408, 1, 0,
              5546614461288261156LL, -1035140312);
    }

    SUBCASE("portalRoomOtherSeed") {
        check(
            run(StrongholdPieceKind::PortalRoom, StructureDirection::North, StrongholdDoor::Grates, Options{.seed = 0}),
            1408, 1, 0, -2178951626122319401LL, 1552052780);
    }
}

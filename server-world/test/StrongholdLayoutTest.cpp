#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/structure/StrongholdLayout.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world::structure;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::string kindName(StrongholdPieceKind kind) {
    switch (kind) {
    case StrongholdPieceKind::Start:
        return "START";
    case StrongholdPieceKind::Straight:
        return "STRAIGHT";
    case StrongholdPieceKind::PrisonHall:
        return "PRISON_HALL";
    case StrongholdPieceKind::LeftTurn:
        return "LEFT_TURN";
    case StrongholdPieceKind::RightTurn:
        return "RIGHT_TURN";
    case StrongholdPieceKind::RoomCrossing:
        return "ROOM_CROSSING";
    case StrongholdPieceKind::StraightStairsDown:
        return "STRAIGHT_STAIRS_DOWN";
    case StrongholdPieceKind::StairsDown:
        return "STAIRS_DOWN";
    case StrongholdPieceKind::FiveCrossing:
        return "FIVE_CROSSING";
    case StrongholdPieceKind::ChestCorridor:
        return "CHEST_CORRIDOR";
    case StrongholdPieceKind::Library:
        return "LIBRARY";
    case StrongholdPieceKind::PortalRoom:
        return "PORTAL_ROOM";
    case StrongholdPieceKind::FillerCorridor:
    default:
        return "FILLER_CORRIDOR";
    }
}

[[nodiscard]] std::string directionName(StructureDirection direction) {
    switch (direction) {
    case StructureDirection::North:
        return "NORTH";
    case StructureDirection::South:
        return "SOUTH";
    case StructureDirection::West:
        return "WEST";
    case StructureDirection::East:
        return "EAST";
    case StructureDirection::Down:
        return "DOWN";
    case StructureDirection::Up:
    default:
        return "UP";
    }
}

[[nodiscard]] std::string doorName(StrongholdDoor door) {
    switch (door) {
    case StrongholdDoor::Opening:
        return "OPENING";
    case StrongholdDoor::WoodDoor:
        return "WOOD_DOOR";
    case StrongholdDoor::Grates:
        return "GRATES";
    case StrongholdDoor::IronDoor:
    default:
        return "IRON_DOOR";
    }
}

[[nodiscard]] std::string flag(bool value) { return value ? "true" : "false"; }

[[nodiscard]] std::string describe(const StrongholdPiece& piece) {
    const StructureBoundingBox& box = piece.box;
    return kindName(piece.kind) + "/" + directionName(piece.orientation) + "/" + doorName(piece.entryDoor) + "/" +
           std::to_string(piece.depth) + "/" + std::to_string(box.minX) + "," + std::to_string(box.minY) + "," +
           std::to_string(box.minZ) + "," + std::to_string(box.maxX) + "," + std::to_string(box.maxY) + "," +
           std::to_string(box.maxZ) + "/" + flag(piece.leftLow) + flag(piece.leftHigh) + flag(piece.rightLow) +
           flag(piece.rightHigh) + "/" + flag(piece.leftChild) + flag(piece.rightChild) + "/" +
           std::to_string(piece.roomType) + "/" + flag(piece.tall) + "/" + std::to_string(piece.steps);
}

struct Run {
    std::int32_t pieces = 0;
    std::int32_t libraries = 0;
    std::int32_t fillers = 0;
    std::string first = "none";
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::int32_t seaLevel, std::int32_t minY, std::int32_t strongholds) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < strongholds; ++index) {
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(0));
        const std::int32_t chunkX = index * 5 - 12;
        const std::int32_t chunkZ = index * -4 + 7;
        const std::vector<StrongholdPiece> built = strongholdLayout::build(
            random,
            [&](std::int32_t attempt) { rng::setLargeFeatureSeed(random, seed + index + attempt, chunkX, chunkZ); },
            chunkX, chunkZ, seaLevel, minY);
        if (index == 0) {
            result.first = built.empty() ? "none" : describe(built.front());
        }
        result.pieces += static_cast<std::int32_t>(built.size());
        for (const StrongholdPiece& piece : built) {
            if (piece.kind == StrongholdPieceKind::Library) {
                ++result.libraries;
            }
            if (piece.kind == StrongholdPieceKind::FillerCorridor) {
                ++result.fillers;
            }
            for (const char letter : describe(piece)) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(letter))) * 1099511628211ULL;
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t pieces, std::int32_t libraries, std::int32_t fillers, std::string_view first,
           std::int64_t checksum) {
    CHECK(result.pieces == pieces);
    CHECK(result.libraries == libraries);
    CHECK(result.fillers == fillers);
    CHECK(result.first == first);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the stronghold maze grows what the frozen kotlin grows") {
    SUBCASE("normal") {
        check(run(31337, 63, -64, 12), 1870, 24, 175,
              "START/NORTH/OPENING/0/-190,25,114,-186,35,118/falsefalsefalsefalse/falsefalse/0/false/0",
              6175142828413896466LL);
    }

    SUBCASE("shallow") {
        check(run(31337, 20, 0, 12), 1870, 24, 175,
              "START/NORTH/OPENING/0/-190,42,114,-186,52,118/falsefalsefalsefalse/falsefalse/0/false/0",
              -5034446036807071571LL);
    }

    SUBCASE("otherSeed") {
        check(run(424242, 63, -64, 12), 1735, 23, 184,
              "START/SOUTH/OPENING/0/-190,17,114,-186,27,118/falsefalsefalsefalse/falsefalse/0/false/0",
              -2481070295196444378LL);
    }
}

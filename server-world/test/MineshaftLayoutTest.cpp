#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/structure/MineshaftLayout.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world::structure;

namespace {

[[nodiscard]] std::string directionName(std::optional<StructureDirection> direction) {
    if (!direction.has_value()) {
        return "-";
    }
    switch (*direction) {
    case StructureDirection::Down:
        return "DOWN";
    case StructureDirection::Up:
        return "UP";
    case StructureDirection::North:
        return "NORTH";
    case StructureDirection::South:
        return "SOUTH";
    case StructureDirection::West:
        return "WEST";
    case StructureDirection::East:
    default:
        return "EAST";
    }
}

[[nodiscard]] std::string kindName(MineshaftPiece::Kind kind) {
    switch (kind) {
    case MineshaftPiece::Kind::Corridor:
        return "CORRIDOR";
    case MineshaftPiece::Kind::Crossing:
        return "CROSSING";
    case MineshaftPiece::Kind::Stairs:
        return "STAIRS";
    case MineshaftPiece::Kind::Room:
    default:
        return "ROOM";
    }
}

[[nodiscard]] std::string boxText(const StructureBoundingBox& box) {
    return std::to_string(box.minX) + "," + std::to_string(box.minY) + "," + std::to_string(box.minZ) + "," +
           std::to_string(box.maxX) + "," + std::to_string(box.maxY) + "," + std::to_string(box.maxZ);
}

[[nodiscard]] std::string describe(const MineshaftPiece& piece) {
    std::string entrances;
    for (const StructureBoundingBox& entrance : piece.entrances) {
        if (!entrances.empty()) {
            entrances += ';';
        }
        entrances += boxText(entrance);
    }
    return kindName(piece.kind) + "/" + directionName(piece.orientation) + "/" +
           directionName(piece.fillOrientation()) + "/" + boxText(piece.box) + "/" + std::to_string(piece.genDepth) +
           "/" + (piece.hasRails ? "true" : "false") + "/" + (piece.spiderCorridor ? "true" : "false") + "/" +
           std::to_string(piece.sections) + "/" + (piece.twoFloored ? "true" : "false") + "/" + entrances;
}

struct Run {
    std::int32_t pieces = 0;
    std::int32_t corridors = 0;
    std::int32_t crossings = 0;
    std::int32_t stairs = 0;
    std::int32_t entrances = 0;
    std::string first = "none";
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, MineshaftType type, std::int32_t seaLevel, std::int32_t minY,
                      std::int32_t shafts) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < shafts; ++index) {
        const std::vector<MineshaftPiece> assembled =
            mineshaftLayout::assemble(seed + index, index * 7 - 20, index * -3 + 11, type, seaLevel, minY);
        if (index == 0) {
            result.first = assembled.empty() ? "none" : describe(assembled.front());
        }
        result.pieces += static_cast<std::int32_t>(assembled.size());
        for (const MineshaftPiece& piece : assembled) {
            switch (piece.kind) {
            case MineshaftPiece::Kind::Corridor:
                ++result.corridors;
                break;
            case MineshaftPiece::Kind::Crossing:
                ++result.crossings;
                break;
            case MineshaftPiece::Kind::Stairs:
                ++result.stairs;
                break;
            case MineshaftPiece::Kind::Room:
                break;
            }
            result.entrances += static_cast<std::int32_t>(piece.entrances.size());
            for (const char letter : describe(piece)) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(letter))) * 1099511628211ULL;
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t pieces, std::int32_t corridors, std::int32_t crossings, std::int32_t stairs,
           std::int32_t entrances, std::string_view first, std::int64_t checksum) {
    CHECK(result.pieces == pieces);
    CHECK(result.corridors == corridors);
    CHECK(result.crossings == crossings);
    CHECK(result.stairs == stairs);
    CHECK(result.entrances == entrances);
    CHECK(result.first == first);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the mineshaft layout lays out what the frozen kotlin lays out") {
    SUBCASE("normal") {
        check(run(8877, MineshaftType::Normal, 63, -64, 24), 2971, 2104, 607, 236, 65,
              "ROOM/-/-/-318,-32,178,-310,-27,186/0/false/false/0/false/"
              "-314,-31,178,-312,-29,179;-314,-36,185,-312,-29,186;-318,-30,179,-317,-28,181;-311,-31,180,-310,-29,182",
              -586347148698501135LL);
    }

    SUBCASE("mesa") {
        check(run(8877, MineshaftType::Mesa, 63, -64, 24), 2971, 2104, 607, 236, 65,
              "ROOM/-/-/-318,50,178,-310,55,186/0/false/false/0/false/"
              "-314,51,178,-312,53,179;-314,46,185,-312,53,186;-318,52,179,-317,54,181;-311,51,180,-310,53,182",
              1646201802744937967LL);
    }

    SUBCASE("shallow") {
        check(run(8877, MineshaftType::Normal, 20, 0, 24), 2971, 2104, 607, 236, 65,
              "ROOM/-/-/-318,10,178,-310,15,186/0/false/false/0/false/"
              "-314,11,178,-312,13,179;-314,6,185,-312,13,186;-318,12,179,-317,14,181;-311,11,180,-310,13,182",
              -1316016122310435987LL);
    }

    SUBCASE("otherSeed") {
        check(run(5150, MineshaftType::Normal, 63, -64, 24), 2833, 2060, 542, 207, 64,
              "ROOM/-/-/-318,24,178,-311,32,190/0/false/false/0/false/"
              "-315,25,178,-311,31,179;-314,26,189,-312,28,190;-312,26,178,-311,28,180",
              -5100485739522927534LL);
    }
}

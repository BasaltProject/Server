#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/structure/MansionPlacer.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::world::structure;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::string rotationName(StructureRotation rotation) {
    switch (rotation) {
    case StructureRotation::None:
        return "NONE";
    case StructureRotation::Clockwise90:
        return "CLOCKWISE_90";
    case StructureRotation::Clockwise180:
        return "CLOCKWISE_180";
    case StructureRotation::Counterclockwise90:
    default:
        return "COUNTERCLOCKWISE_90";
    }
}

[[nodiscard]] std::string mirrorName(StructureMirror mirror) {
    switch (mirror) {
    case StructureMirror::LeftRight:
        return "LEFT_RIGHT";
    case StructureMirror::FrontBack:
        return "FRONT_BACK";
    case StructureMirror::None:
    default:
        return "NONE";
    }
}

[[nodiscard]] std::string describe(const MansionPiece& piece) {
    return piece.templateName + "@" + std::to_string(piece.position.x) + "," + std::to_string(piece.position.y) + "," +
           std::to_string(piece.position.z) + "/" + rotationName(piece.rotation) + "/" + mirrorName(piece.mirror);
}

struct Run {
    std::int32_t pieces = 0;
    std::size_t templates = 0;
    std::string first = "none";
    std::int32_t draws = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, StructureRotation rotation, std::int32_t mansions) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    std::uint32_t draws = 0;
    std::set<std::string> names;
    const auto foldGrid = [&checksum](const SimpleGrid& grid) {
        for (std::int32_t y = 0; y < grid.height(); ++y) {
            for (std::int32_t x = 0; x < grid.width(); ++x) {
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(grid.at(x, y)))) *
                           1099511628211ULL;
            }
        }
    };
    for (std::int32_t index = 0; index < mansions; ++index) {
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed + index));
        const MansionGrid grid{random};
        foldGrid(grid.baseGrid());
        foldGrid(grid.thirdFloorGrid());
        for (std::size_t floor = 0; floor < 3; ++floor) {
            foldGrid(grid.floorRooms(floor));
        }
        std::vector<MansionPiece> list;
        mansionPlacer::createMansion(list, random, StructurePos{index * 3 - 10, 64, index * -2 + 7}, rotation, grid);
        if (index == 0) {
            result.first = list.empty() ? "none" : describe(list.front());
        }
        result.pieces += static_cast<std::int32_t>(list.size());
        for (const MansionPiece& piece : list) {
            names.insert(piece.templateName);
            for (const char letter : describe(piece)) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(letter))) * 1099511628211ULL;
            }
        }
        draws = draws * 31U + static_cast<std::uint32_t>(random.nextInt(1 << 20));
    }
    result.templates = names.size();
    result.draws = static_cast<std::int32_t>(draws);
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t pieces, std::size_t templates, std::string_view first, std::int32_t draws,
           std::int64_t checksum) {
    CHECK(result.pieces == pieces);
    CHECK(result.templates == templates);
    CHECK(result.first == first);
    CHECK(result.draws == draws);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the woodland mansion comes out as the frozen kotlin's") {
    SUBCASE("north") {
        check(run(90210, StructureRotation::None, 16), 9054, 71, "entrance@-19,64,7/NONE/NONE", 1349025964,
              -3883814432552477167LL);
    }

    SUBCASE("clockwise") {
        check(run(90210, StructureRotation::Clockwise90, 16), 9054, 71, "entrance@-10,64,-2/CLOCKWISE_90/NONE",
              1349025964, 3106308662260725663LL);
    }

    SUBCASE("otherSeed") {
        check(run(13, StructureRotation::None, 16), 8832, 71, "entrance@-19,64,7/NONE/NONE", 2147316012,
              -8045497231439304255LL);
    }
}

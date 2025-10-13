#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/structure/StructureQueries.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>

using namespace basalt::server::world::structure;

namespace {

constexpr std::array<StructureMirror, 3> kMirrors{StructureMirror::None, StructureMirror::LeftRight,
                                                  StructureMirror::FrontBack};
constexpr std::array<StructureRotation, 4> kRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                      StructureRotation::Clockwise180,
                                                      StructureRotation::Counterclockwise90};
constexpr std::array<StructureDirection, 6> kDirections{StructureDirection::Down,  StructureDirection::Up,
                                                        StructureDirection::North, StructureDirection::South,
                                                        StructureDirection::West,  StructureDirection::East};

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

[[nodiscard]] std::int64_t transformChecksum(StructureMirror mirror, StructureRotation rotation) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 512; ++index) {
        const StructurePos pos{floorMod(index * 7, 23) - 11, floorMod(index * 3, 17) - 8,
                               floorMod(index * 11, 29) - 14};
        const StructurePos pivot{floorMod(index * 5, 9) - 4, 0, floorMod(index * 13, 9) - 4};
        const StructurePos moved = transform(pos, mirror, rotation, pivot);
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(moved.x))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(moved.y))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(moved.z))) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

/// The Kotlin printed enum names; upper-casing here keeps one expected string for both sides.
[[nodiscard]] std::string upper(std::string_view name) {
    std::string out{name};
    for (char& character : out) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 32);
        }
    }
    return out;
}

} // namespace

TEST_CASE("the structure transform matches the frozen kotlin") {
    SUBCASE("NONE_NONE") {
        CHECK(transformChecksum(StructureMirror::None, StructureRotation::None) == 1655666826068964224LL);
    }

    SUBCASE("NONE_CLOCKWISE_90") {
        CHECK(transformChecksum(StructureMirror::None, StructureRotation::Clockwise90) == -6358800883888846350LL);
    }

    SUBCASE("NONE_CLOCKWISE_180") {
        CHECK(transformChecksum(StructureMirror::None, StructureRotation::Clockwise180) == -9172096633135981754LL);
    }

    SUBCASE("NONE_COUNTERCLOCKWISE_90") {
        CHECK(transformChecksum(StructureMirror::None, StructureRotation::Counterclockwise90) ==
              -8040915643247354036LL);
    }

    SUBCASE("LEFT_RIGHT_NONE") {
        CHECK(transformChecksum(StructureMirror::LeftRight, StructureRotation::None) == 9035874919195974542LL);
    }

    SUBCASE("LEFT_RIGHT_CLOCKWISE_90") {
        CHECK(transformChecksum(StructureMirror::LeftRight, StructureRotation::Clockwise90) == 390450170246996076LL);
    }

    SUBCASE("LEFT_RIGHT_CLOCKWISE_180") {
        CHECK(transformChecksum(StructureMirror::LeftRight, StructureRotation::Clockwise180) == -6551247859448010852LL);
    }

    SUBCASE("LEFT_RIGHT_COUNTERCLOCKWISE_90") {
        CHECK(transformChecksum(StructureMirror::LeftRight, StructureRotation::Counterclockwise90) ==
              -5814885713162809818LL);
    }

    SUBCASE("FRONT_BACK_NONE") {
        CHECK(transformChecksum(StructureMirror::FrontBack, StructureRotation::None) == 2112307668033248822LL);
    }

    SUBCASE("FRONT_BACK_CLOCKWISE_90") {
        CHECK(transformChecksum(StructureMirror::FrontBack, StructureRotation::Clockwise90) == -3574207970471757548LL);
    }

    SUBCASE("FRONT_BACK_CLOCKWISE_180") {
        CHECK(transformChecksum(StructureMirror::FrontBack, StructureRotation::Clockwise180) == 3364317504950825144LL);
    }

    SUBCASE("FRONT_BACK_COUNTERCLOCKWISE_90") {
        CHECK(transformChecksum(StructureMirror::FrontBack, StructureRotation::Counterclockwise90) ==
              -6336295519700022318LL);
    }
}

TEST_CASE("the structure directions match the frozen kotlin") {
    std::string rows;
    for (const StructureDirection direction : kDirections) {
        rows += upper(serializedName(direction)) + ":";
        rows +=
            std::to_string(dx(direction)) + "," + std::to_string(dy(direction)) + "," + std::to_string(dz(direction));
        rows += std::string{"/"} + (isHorizontal(direction) ? "true" : "false");
        rows += "/" + upper(serializedName(opposite(direction)));
        for (const StructureMirror mirror : kMirrors) {
            rows += "/" + upper(serializedName(mirrored(direction, mirror)));
        }
        for (const StructureRotation rotation : kRotations) {
            rows += "/" + upper(serializedName(rotated(direction, rotation)));
        }
        rows += ";";
    }
    CHECK(rows == "DOWN:0,-1,0/false/UP/DOWN/DOWN/DOWN/DOWN/DOWN/DOWN/DOWN;UP:0,1,0/false/DOWN/UP/UP/UP/UP/UP/UP/UP;NOR"
                  "TH:0,0,-1/true/SOUTH/NORTH/SOUTH/NORTH/NORTH/EAST/SOUTH/WEST;SOUTH:0,0,1/true/NORTH/SOUTH/NORTH/SOUT"
                  "H/SOUTH/WEST/NORTH/EAST;WEST:-1,0,0/true/EAST/WEST/WEST/EAST/WEST/NORTH/EAST/SOUTH;EAST:1,0,0/true/W"
                  "EST/EAST/EAST/WEST/EAST/SOUTH/WEST/NORTH;");

    std::string rotationNames;
    for (const StructureRotation rotation : kRotations) {
        if (!rotationNames.empty()) {
            rotationNames += ",";
        }
        rotationNames += serializedName(rotation);
    }
    CHECK(rotationNames == "none,clockwise_90,clockwise_180,counterclockwise_90");

    std::string mirrorNames;
    for (const StructureMirror mirror : kMirrors) {
        if (!mirrorNames.empty()) {
            mirrorNames += ",";
        }
        mirrorNames += serializedName(mirror);
    }
    CHECK(mirrorNames == "none,left_right,front_back");

    CHECK(directionFromName("NoRtH").value() == StructureDirection::North);
    CHECK_FALSE(directionFromName("nope").has_value());
}

TEST_CASE("the structure position queries match the frozen kotlin") {
    const auto box = [](std::int32_t minX, std::int32_t minY, std::int32_t minZ, std::int32_t maxX, std::int32_t maxY,
                        std::int32_t maxZ) { return StructureBoundingBox{minX, minY, minZ, maxX, maxY, maxZ}; };
    const StructureStartRecord fortress =
        StructureStartRecord::of("fortress", 0, 0, {box(0, 60, 0, 8, 70, 8), box(20, 60, 0, 28, 70, 8)}).value();
    const StructureStartRecord city =
        StructureStartRecord::of("ancient_city", 0, 0, {box(4, 55, 4, 12, 65, 12)}).value();
    ChunkStructures chunk;
    CHECK(chunk.add(fortress));
    CHECK(chunk.add(city));
    // The same start offered twice is dropped rather than duplicated.
    CHECK_FALSE(chunk.add(fortress));
    const StructureQueries queries{
        [&chunk](std::int32_t x, std::int32_t z) { return x == 0 && z == 0 ? &chunk : nullptr; }};
    const std::array<StructurePos, 5> probes{StructurePos{5, 62, 5}, StructurePos{15, 62, 5}, StructurePos{24, 62, 5},
                                             StructurePos{5, 58, 5}, StructurePos{100, 62, 100}};
    std::string rows;
    for (const StructurePos& pos : probes) {
        rows += std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z) + ":";
        rows += std::to_string(queries.startsIn(pos).size());
        for (const std::string_view structure : {"fortress", "ancient_city"}) {
            const StructureStartRecord* atStart = queries.startAt(pos, structure);
            const StructureStartRecord* atPiece = queries.startWithPieceAt(pos, structure);
            if (structure == "fortress") {
                rows += "/" + (atStart == nullptr ? std::string{"-"} : atStart->structure);
                rows += "/" + (atPiece == nullptr ? std::string{"-"} : atPiece->structure);
            }
        }
        const StructureStartRecord* cityStart = queries.startAt(pos, "ancient_city");
        const StructureStartRecord* cityPiece = queries.startWithPieceAt(pos, "ancient_city");
        rows += "/" + (cityStart == nullptr ? std::string{"-"} : cityStart->structure);
        rows += "/" + (cityPiece == nullptr ? std::string{"-"} : cityPiece->structure);
        rows += ";";
    }
    CHECK(rows == "5,62,5:2/fortress/fortress/ancient_city/ancient_city;15,62,5:2/fortress/-/-/-;24,62,5:0/-/-/-/"
                  "-;5,58,5:2/-/-/ancient_city/ancient_city;100,62,100:0/-/-/-/-;");
    CHECK(chunk.starts().size() == 2);
    CHECK(chunk.has("fortress"));
    CHECK_FALSE(chunk.has("igloo"));
    CHECK_FALSE(chunk.empty());
    CHECK(fortress.fullBounds.minX == 0);
    CHECK(fortress.fullBounds.maxX == 28);
}

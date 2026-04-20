#include <basalt/server/world/structure/MonumentSingleRooms.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::size_t ordinal(StructureDirection direction) { return static_cast<std::size_t>(direction); }

[[nodiscard]] bool opening(const MonumentRoom& room, StructureDirection direction) {
    return room.hasOpening(ordinal(direction));
}

[[nodiscard]] bool aboveGround(const MonumentRoom& room) { return room.index() / 25 > 0; }

void simpleCorners(const MonumentBrush& brush, const MonumentRoom& room) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.box(0, 1, 0, 2, 1, 2, light);
    brush.box(0, 3, 0, 2, 3, 2, light);
    brush.box(0, 2, 0, 0, 2, 2, gray);
    brush.box(1, 2, 0, 2, 2, 0, gray);
    brush.place(lamp, 1, 2, 1);
    brush.box(5, 1, 0, 7, 1, 2, light);
    brush.box(5, 3, 0, 7, 3, 2, light);
    brush.box(7, 2, 0, 7, 2, 2, gray);
    brush.box(5, 2, 0, 6, 2, 0, gray);
    brush.place(lamp, 6, 2, 1);
    brush.box(0, 1, 5, 2, 1, 7, light);
    brush.box(0, 3, 5, 2, 3, 7, light);
    brush.box(0, 2, 5, 0, 2, 7, gray);
    brush.box(1, 2, 7, 2, 2, 7, gray);
    brush.place(lamp, 1, 2, 6);
    brush.box(5, 1, 5, 7, 1, 7, light);
    brush.box(5, 3, 5, 7, 3, 7, light);
    brush.box(7, 2, 5, 7, 2, 7, gray);
    brush.box(5, 2, 7, 6, 2, 7, gray);
    brush.place(lamp, 6, 2, 6);
    if (opening(room, StructureDirection::South)) {
        brush.box(3, 3, 0, 4, 3, 0, light);
    } else {
        brush.box(3, 3, 0, 4, 3, 1, light);
        brush.box(3, 2, 0, 4, 2, 0, gray);
        brush.box(3, 1, 0, 4, 1, 1, light);
    }
    if (opening(room, StructureDirection::North)) {
        brush.box(3, 3, 7, 4, 3, 7, light);
    } else {
        brush.box(3, 3, 6, 4, 3, 7, light);
        brush.box(3, 2, 7, 4, 2, 7, gray);
        brush.box(3, 1, 6, 4, 1, 7, light);
    }
    if (opening(room, StructureDirection::West)) {
        brush.box(0, 3, 3, 0, 3, 4, light);
    } else {
        brush.box(0, 3, 3, 1, 3, 4, light);
        brush.box(0, 2, 3, 0, 2, 4, gray);
        brush.box(0, 1, 3, 1, 1, 4, light);
    }
    if (opening(room, StructureDirection::East)) {
        brush.box(7, 3, 3, 7, 3, 4, light);
    } else {
        brush.box(6, 3, 3, 7, 3, 4, light);
        brush.box(7, 2, 3, 7, 2, 4, gray);
        brush.box(6, 1, 3, 7, 1, 4, light);
    }
}

void simpleColumns(const MonumentBrush& brush, const MonumentRoom& room) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.box(2, 1, 2, 2, 3, 2, light);
    brush.box(2, 1, 5, 2, 3, 5, light);
    brush.box(5, 1, 5, 5, 3, 5, light);
    brush.box(5, 1, 2, 5, 3, 2, light);
    brush.place(lamp, 2, 2, 2);
    brush.place(lamp, 2, 2, 5);
    brush.place(lamp, 5, 2, 5);
    brush.place(lamp, 5, 2, 2);
    brush.box(0, 1, 0, 1, 3, 0, light);
    brush.box(0, 1, 1, 0, 3, 1, light);
    brush.box(0, 1, 7, 1, 3, 7, light);
    brush.box(0, 1, 6, 0, 3, 6, light);
    brush.box(6, 1, 7, 7, 3, 7, light);
    brush.box(7, 1, 6, 7, 3, 6, light);
    brush.box(6, 1, 0, 7, 3, 0, light);
    brush.box(7, 1, 1, 7, 3, 1, light);
    brush.place(gray, 1, 2, 0);
    brush.place(gray, 0, 2, 1);
    brush.place(gray, 1, 2, 7);
    brush.place(gray, 0, 2, 6);
    brush.place(gray, 6, 2, 7);
    brush.place(gray, 7, 2, 6);
    brush.place(gray, 6, 2, 0);
    brush.place(gray, 7, 2, 1);
    if (!opening(room, StructureDirection::South)) {
        brush.box(1, 3, 0, 6, 3, 0, light);
        brush.box(1, 2, 0, 6, 2, 0, gray);
        brush.box(1, 1, 0, 6, 1, 0, light);
    }
    if (!opening(room, StructureDirection::North)) {
        brush.box(1, 3, 7, 6, 3, 7, light);
        brush.box(1, 2, 7, 6, 2, 7, gray);
        brush.box(1, 1, 7, 6, 1, 7, light);
    }
    if (!opening(room, StructureDirection::West)) {
        brush.box(0, 3, 1, 0, 3, 6, light);
        brush.box(0, 2, 1, 0, 2, 6, gray);
        brush.box(0, 1, 1, 0, 1, 6, light);
    }
    if (!opening(room, StructureDirection::East)) {
        brush.box(7, 3, 1, 7, 3, 6, light);
        brush.box(7, 2, 1, 7, 2, 6, gray);
        brush.box(7, 1, 1, 7, 1, 6, light);
    }
}

void bandedShell(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t black = monumentPalette::baseBlack();
    for (const auto& [y, block] :
         std::array<std::pair<std::int32_t, std::int32_t>, 3>{{{1, light}, {2, black}, {3, light}}}) {
        brush.box(0, y, 0, 0, y, 7, block);
        brush.box(7, y, 0, 7, y, 7, block);
        brush.box(1, y, 0, 6, y, 0, block);
        brush.box(1, y, 7, 6, y, 7, block);
    }
    brush.box(0, 1, 3, 0, 2, 4, black);
    brush.box(7, 1, 3, 7, 2, 4, black);
    brush.box(3, 1, 0, 4, 2, 0, black);
    brush.box(3, 1, 7, 4, 2, 7, black);
}

void simpleBanded(const MonumentBrush& brush, const MonumentRoom& room) {
    bandedShell(brush);
    if (opening(room, StructureDirection::South)) {
        brush.waterBox(3, 1, 0, 4, 2, 0);
    }
    if (opening(room, StructureDirection::North)) {
        brush.waterBox(3, 1, 7, 4, 2, 7);
    }
    if (opening(room, StructureDirection::West)) {
        brush.waterBox(0, 1, 3, 0, 2, 4);
    }
    if (opening(room, StructureDirection::East)) {
        brush.waterBox(7, 1, 3, 7, 2, 4);
    }
}

} // namespace

void monumentSingleRooms::entry(const MonumentBrush& brush, const MonumentRoom& room) {
    const std::int32_t light = monumentPalette::baseLight();
    brush.box(0, 3, 0, 2, 3, 7, light);
    brush.box(5, 3, 0, 7, 3, 7, light);
    brush.box(0, 2, 0, 1, 2, 7, light);
    brush.box(6, 2, 0, 7, 2, 7, light);
    brush.box(0, 1, 0, 0, 1, 7, light);
    brush.box(7, 1, 0, 7, 1, 7, light);
    brush.box(0, 1, 7, 7, 3, 7, light);
    brush.box(1, 1, 0, 2, 3, 0, light);
    brush.box(5, 1, 0, 6, 3, 0, light);
    if (opening(room, StructureDirection::North)) {
        brush.waterBox(3, 1, 7, 4, 2, 7);
    }
    if (opening(room, StructureDirection::West)) {
        brush.waterBox(0, 1, 3, 1, 2, 4);
    }
    if (opening(room, StructureDirection::East)) {
        brush.waterBox(6, 1, 3, 7, 2, 4);
    }
}

void monumentSingleRooms::simple(const MonumentBrush& brush, const MonumentRoom& room, std::int32_t design,
                                 random::RandomSource& random) {
    if (aboveGround(room)) {
        brush.defaultFloor(0, 0, opening(room, StructureDirection::Down));
    }
    if (room.connection(ordinal(StructureDirection::Up)) == nullptr) {
        brush.boxOnFillOnly(1, 4, 1, 6, 4, 6, monumentPalette::baseGray());
    }
    const bool centrePillar = design != 0 && random.nextBoolean() && !opening(room, StructureDirection::Down) &&
                              !opening(room, StructureDirection::Up) && room.countOpenings() > 1;
    switch (design) {
    case 0:
        simpleCorners(brush, room);
        break;
    case 1:
        simpleColumns(brush, room);
        break;
    case 2:
        simpleBanded(brush, room);
        break;
    default:
        break;
    }
    if (centrePillar) {
        brush.box(3, 1, 3, 4, 1, 4, monumentPalette::baseLight());
        brush.box(3, 2, 3, 4, 2, 4, monumentPalette::baseGray());
        brush.box(3, 3, 3, 4, 3, 4, monumentPalette::baseLight());
    }
}

void monumentSingleRooms::simpleTop(const MonumentBrush& brush, const MonumentRoom& room,
                                    random::RandomSource& random) {
    if (aboveGround(room)) {
        brush.defaultFloor(0, 0, opening(room, StructureDirection::Down));
    }
    if (room.connection(ordinal(StructureDirection::Up)) == nullptr) {
        brush.boxOnFillOnly(1, 4, 1, 6, 4, 6, monumentPalette::baseGray());
    }
    for (std::int32_t x = 1; x <= 6; ++x) {
        for (std::int32_t z = 1; z <= 6; ++z) {
            if (random.nextInt(3) == 0) {
                continue;
            }
            const std::int32_t y0 = 2 + (random.nextInt(4) == 0 ? 0 : 1);
            brush.box(x, y0, z, x, 3, z, monumentPalette::wetSponge());
        }
    }
    bandedShell(brush);
    if (opening(room, StructureDirection::South)) {
        brush.waterBox(3, 1, 0, 4, 2, 0);
    }
}

void monumentSingleRooms::core(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t black = monumentPalette::baseBlack();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.boxOnFillOnly(1, 8, 0, 14, 8, 14, gray);
    brush.box(0, 7, 0, 0, 7, 15, light);
    brush.box(15, 7, 0, 15, 7, 15, light);
    brush.box(1, 7, 0, 15, 7, 0, light);
    brush.box(1, 7, 15, 14, 7, 15, light);
    for (std::int32_t y = 1; y <= 6; ++y) {
        const std::int32_t block = y == 2 || y == 6 ? gray : light;
        for (std::int32_t x = 0; x <= 15; x += 15) {
            brush.box(x, y, 0, x, y, 1, block);
            brush.box(x, y, 6, x, y, 9, block);
            brush.box(x, y, 14, x, y, 15, block);
        }
        brush.box(1, y, 0, 1, y, 0, block);
        brush.box(6, y, 0, 9, y, 0, block);
        brush.box(14, y, 0, 14, y, 0, block);
        brush.box(1, y, 15, 14, y, 15, block);
    }
    brush.box(6, 3, 6, 9, 6, 9, black);
    brush.box(7, 4, 7, 8, 5, 8, monumentPalette::gold());
    for (std::int32_t y = 3; y <= 6; y += 3) {
        for (std::int32_t x = 6; x <= 9; x += 3) {
            brush.place(lamp, x, y, 6);
            brush.place(lamp, x, y, 9);
        }
    }
    brush.box(5, 1, 6, 5, 2, 6, light);
    brush.box(5, 1, 9, 5, 2, 9, light);
    brush.box(10, 1, 6, 10, 2, 6, light);
    brush.box(10, 1, 9, 10, 2, 9, light);
    brush.box(6, 1, 5, 6, 2, 5, light);
    brush.box(9, 1, 5, 9, 2, 5, light);
    brush.box(6, 1, 10, 6, 2, 10, light);
    brush.box(9, 1, 10, 9, 2, 10, light);
    brush.box(5, 2, 5, 5, 6, 5, light);
    brush.box(5, 2, 10, 5, 6, 10, light);
    brush.box(10, 2, 5, 10, 6, 5, light);
    brush.box(10, 2, 10, 10, 6, 10, light);
    brush.box(5, 7, 1, 5, 7, 6, light);
    brush.box(10, 7, 1, 10, 7, 6, light);
    brush.box(5, 7, 9, 5, 7, 14, light);
    brush.box(10, 7, 9, 10, 7, 14, light);
    brush.box(1, 7, 5, 6, 7, 5, light);
    brush.box(1, 7, 10, 6, 7, 10, light);
    brush.box(9, 7, 5, 14, 7, 5, light);
    brush.box(9, 7, 10, 14, 7, 10, light);
    brush.box(2, 1, 2, 2, 1, 3, light);
    brush.box(3, 1, 2, 3, 1, 2, light);
    brush.box(13, 1, 2, 13, 1, 3, light);
    brush.box(12, 1, 2, 12, 1, 2, light);
    brush.box(2, 1, 12, 2, 1, 13, light);
    brush.box(3, 1, 13, 3, 1, 13, light);
    brush.box(13, 1, 12, 13, 1, 13, light);
    brush.box(12, 1, 13, 12, 1, 13, light);
}

} // namespace basalt::server::world::structure

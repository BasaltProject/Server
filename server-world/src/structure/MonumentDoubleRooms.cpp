#include <basalt/server/world/structure/MonumentDoubleRooms.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::size_t ordinal(StructureDirection direction) { return static_cast<std::size_t>(direction); }

[[nodiscard]] bool opening(const MonumentRoom& room, StructureDirection direction) {
    return room.hasOpening(ordinal(direction));
}

[[nodiscard]] MonumentRoom* up(const MonumentRoom& room) { return room.connection(ordinal(StructureDirection::Up)); }

/// A room on the ground floor sits on the monument's own base, so it lays no floor of its own.
[[nodiscard]] bool aboveGround(const MonumentRoom& room) { return room.index() / 25 > 0; }

/// The doorways of the lower pair, west half on the left and east half on the right.
void lowerDoors(const MonumentBrush& brush, const MonumentRoom& west, const MonumentRoom& east) {
    if (opening(west, StructureDirection::South)) {
        brush.waterBox(3, 1, 0, 4, 2, 0);
    }
    if (opening(west, StructureDirection::North)) {
        brush.waterBox(3, 1, 7, 4, 2, 7);
    }
    if (opening(west, StructureDirection::West)) {
        brush.waterBox(0, 1, 3, 0, 2, 4);
    }
    if (opening(east, StructureDirection::South)) {
        brush.waterBox(11, 1, 0, 12, 2, 0);
    }
    if (opening(east, StructureDirection::North)) {
        brush.waterBox(11, 1, 7, 12, 2, 7);
    }
    if (opening(east, StructureDirection::East)) {
        brush.waterBox(15, 1, 3, 15, 2, 4);
    }
}

void upperDoors(const MonumentBrush& brush, const MonumentRoom& westUp, const MonumentRoom& eastUp) {
    if (opening(westUp, StructureDirection::South)) {
        brush.waterBox(3, 5, 0, 4, 6, 0);
    }
    if (opening(westUp, StructureDirection::North)) {
        brush.waterBox(3, 5, 7, 4, 6, 7);
    }
    if (opening(westUp, StructureDirection::West)) {
        brush.waterBox(0, 5, 3, 0, 6, 4);
    }
    if (opening(eastUp, StructureDirection::South)) {
        brush.waterBox(11, 5, 0, 12, 6, 0);
    }
    if (opening(eastUp, StructureDirection::North)) {
        brush.waterBox(11, 5, 7, 12, 6, 7);
    }
    if (opening(eastUp, StructureDirection::East)) {
        brush.waterBox(15, 5, 3, 15, 6, 4);
    }
}

void wallOrDoor(const MonumentBrush& brush, const MonumentRoom& room, StructureDirection direction, std::int32_t y,
                std::int32_t z) {
    const std::int32_t light = monumentPalette::baseLight();
    if (opening(room, direction)) {
        brush.box(2, y, z, 2, y + 2, z, light);
        brush.box(5, y, z, 5, y + 2, z, light);
        brush.box(3, y + 2, z, 4, y + 2, z, light);
    } else {
        brush.box(0, y, z, 7, y + 2, z, light);
        brush.box(0, y + 1, z, 7, y + 1, z, monumentPalette::baseGray());
    }
}

void sideWallOrDoor(const MonumentBrush& brush, const MonumentRoom& room, StructureDirection direction, std::int32_t y,
                    std::int32_t x) {
    const std::int32_t light = monumentPalette::baseLight();
    if (opening(room, direction)) {
        brush.box(x, y, 2, x, y + 2, 2, light);
        brush.box(x, y, 5, x, y + 2, 5, light);
        brush.box(x, y + 2, 3, x, y + 2, 4, light);
    } else {
        brush.box(x, y, 0, x, y + 2, 7, light);
        brush.box(x, y + 1, 0, x, y + 1, 7, monumentPalette::baseGray());
    }
}

} // namespace

void monumentDoubleRooms::doubleX(const MonumentBrush& brush, const MonumentRoom& west) {
    const MonumentRoom* east = west.connection(ordinal(StructureDirection::East));
    if (east == nullptr) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    if (aboveGround(west)) {
        brush.defaultFloor(8, 0, opening(*east, StructureDirection::Down));
        brush.defaultFloor(0, 0, opening(west, StructureDirection::Down));
    }
    if (up(west) == nullptr) {
        brush.boxOnFillOnly(1, 4, 1, 7, 4, 6, gray);
    }
    if (up(*east) == nullptr) {
        brush.boxOnFillOnly(8, 4, 1, 14, 4, 6, gray);
    }
    for (const auto& [y, block] :
         std::array<std::pair<std::int32_t, std::int32_t>, 3>{{{3, light}, {2, gray}, {1, light}}}) {
        brush.box(0, y, 0, 0, y, 7, block);
        brush.box(15, y, 0, 15, y, 7, block);
        brush.box(1, y, 0, 15, y, 0, block);
        brush.box(1, y, 7, 14, y, 7, block);
    }
    brush.box(5, 1, 0, 10, 1, 4, light);
    brush.box(6, 2, 0, 9, 2, 3, gray);
    brush.box(5, 3, 0, 10, 3, 4, light);
    brush.place(monumentPalette::lamp(), 6, 2, 3);
    brush.place(monumentPalette::lamp(), 9, 2, 3);
    lowerDoors(brush, west, *east);
}

void monumentDoubleRooms::doubleXy(const MonumentBrush& brush, const MonumentRoom& west) {
    const MonumentRoom* east = west.connection(ordinal(StructureDirection::East));
    if (east == nullptr) {
        return;
    }
    const MonumentRoom* westUp = up(west);
    const MonumentRoom* eastUp = up(*east);
    if (westUp == nullptr || eastUp == nullptr) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t lamp = monumentPalette::lamp();
    if (aboveGround(west)) {
        brush.defaultFloor(8, 0, opening(*east, StructureDirection::Down));
        brush.defaultFloor(0, 0, opening(west, StructureDirection::Down));
    }
    if (up(*westUp) == nullptr) {
        brush.boxOnFillOnly(1, 8, 1, 7, 8, 6, gray);
    }
    if (up(*eastUp) == nullptr) {
        brush.boxOnFillOnly(8, 8, 1, 14, 8, 6, gray);
    }
    for (std::int32_t y = 1; y <= 7; ++y) {
        const std::int32_t block = y == 2 || y == 6 ? gray : light;
        brush.box(0, y, 0, 0, y, 7, block);
        brush.box(15, y, 0, 15, y, 7, block);
        brush.box(1, y, 0, 15, y, 0, block);
        brush.box(1, y, 7, 14, y, 7, block);
    }
    brush.box(2, 1, 3, 2, 7, 4, light);
    brush.box(3, 1, 2, 4, 7, 2, light);
    brush.box(3, 1, 5, 4, 7, 5, light);
    brush.box(13, 1, 3, 13, 7, 4, light);
    brush.box(11, 1, 2, 12, 7, 2, light);
    brush.box(11, 1, 5, 12, 7, 5, light);
    brush.box(5, 1, 3, 5, 3, 4, light);
    brush.box(10, 1, 3, 10, 3, 4, light);
    brush.box(5, 7, 2, 10, 7, 5, light);
    brush.box(5, 5, 2, 5, 7, 2, light);
    brush.box(10, 5, 2, 10, 7, 2, light);
    brush.box(5, 5, 5, 5, 7, 5, light);
    brush.box(10, 5, 5, 10, 7, 5, light);
    brush.place(light, 6, 6, 2);
    brush.place(light, 9, 6, 2);
    brush.place(light, 6, 6, 5);
    brush.place(light, 9, 6, 5);
    brush.box(5, 4, 3, 6, 4, 4, light);
    brush.box(9, 4, 3, 10, 4, 4, light);
    brush.place(lamp, 5, 4, 2);
    brush.place(lamp, 5, 4, 5);
    brush.place(lamp, 10, 4, 2);
    brush.place(lamp, 10, 4, 5);
    lowerDoors(brush, west, *east);
    upperDoors(brush, *westUp, *eastUp);
}

void monumentDoubleRooms::doubleY(const MonumentBrush& brush, const MonumentRoom& lower) {
    const std::int32_t light = monumentPalette::baseLight();
    if (aboveGround(lower)) {
        brush.defaultFloor(0, 0, opening(lower, StructureDirection::Down));
    }
    const MonumentRoom* above = up(lower);
    if (above == nullptr) {
        return;
    }
    if (up(*above) == nullptr) {
        brush.boxOnFillOnly(1, 8, 1, 6, 8, 6, monumentPalette::baseGray());
    }
    brush.box(0, 4, 0, 0, 4, 7, light);
    brush.box(7, 4, 0, 7, 4, 7, light);
    brush.box(1, 4, 0, 6, 4, 0, light);
    brush.box(1, 4, 7, 6, 4, 7, light);
    brush.box(2, 4, 1, 2, 4, 2, light);
    brush.box(1, 4, 2, 1, 4, 2, light);
    brush.box(5, 4, 1, 5, 4, 2, light);
    brush.box(6, 4, 2, 6, 4, 2, light);
    brush.box(2, 4, 5, 2, 4, 6, light);
    brush.box(1, 4, 5, 1, 4, 5, light);
    brush.box(5, 4, 5, 5, 4, 6, light);
    brush.box(6, 4, 5, 6, 4, 5, light);
    // The lower storey draws its four walls, then the upper one draws the same four again.
    const MonumentRoom* storey = &lower;
    for (std::int32_t y = 1; y <= 5; y += 4) {
        wallOrDoor(brush, *storey, StructureDirection::South, y, 0);
        wallOrDoor(brush, *storey, StructureDirection::North, y, 7);
        sideWallOrDoor(brush, *storey, StructureDirection::West, y, 0);
        sideWallOrDoor(brush, *storey, StructureDirection::East, y, 7);
        storey = above;
    }
}

void monumentDoubleRooms::doubleYz(const MonumentBrush& brush, const MonumentRoom& south) {
    const MonumentRoom* north = south.connection(ordinal(StructureDirection::North));
    if (north == nullptr) {
        return;
    }
    const MonumentRoom* northUp = up(*north);
    const MonumentRoom* southUp = up(south);
    if (northUp == nullptr || southUp == nullptr) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    if (aboveGround(south)) {
        brush.defaultFloor(0, 8, opening(*north, StructureDirection::Down));
        brush.defaultFloor(0, 0, opening(south, StructureDirection::Down));
    }
    if (up(*southUp) == nullptr) {
        brush.boxOnFillOnly(1, 8, 1, 6, 8, 7, gray);
    }
    if (up(*northUp) == nullptr) {
        brush.boxOnFillOnly(1, 8, 8, 6, 8, 14, gray);
    }
    for (std::int32_t y = 1; y <= 7; ++y) {
        const std::int32_t block = y == 2 || y == 6 ? gray : light;
        brush.box(0, y, 0, 0, y, 15, block);
        brush.box(7, y, 0, 7, y, 15, block);
        brush.box(1, y, 0, 6, y, 0, block);
        brush.box(1, y, 15, 6, y, 15, block);
    }
    // The pillar between the two halves is dark prismarine banded with two courses of lantern.
    for (std::int32_t y = 1; y <= 7; ++y) {
        brush.box(3, y, 7, 4, y, 8, y == 2 || y == 6 ? monumentPalette::lamp() : monumentPalette::baseBlack());
    }
    if (opening(south, StructureDirection::South)) {
        brush.waterBox(3, 1, 0, 4, 2, 0);
    }
    if (opening(south, StructureDirection::East)) {
        brush.waterBox(7, 1, 3, 7, 2, 4);
    }
    if (opening(south, StructureDirection::West)) {
        brush.waterBox(0, 1, 3, 0, 2, 4);
    }
    if (opening(*north, StructureDirection::North)) {
        brush.waterBox(3, 1, 15, 4, 2, 15);
    }
    if (opening(*north, StructureDirection::West)) {
        brush.waterBox(0, 1, 11, 0, 2, 12);
    }
    if (opening(*north, StructureDirection::East)) {
        brush.waterBox(7, 1, 11, 7, 2, 12);
    }
    if (opening(*southUp, StructureDirection::South)) {
        brush.waterBox(3, 5, 0, 4, 6, 0);
    }
    if (opening(*southUp, StructureDirection::East)) {
        brush.waterBox(7, 5, 3, 7, 6, 4);
        brush.box(5, 4, 2, 6, 4, 5, light);
        brush.box(6, 1, 2, 6, 3, 2, light);
        brush.box(6, 1, 5, 6, 3, 5, light);
    }
    if (opening(*southUp, StructureDirection::West)) {
        brush.waterBox(0, 5, 3, 0, 6, 4);
        brush.box(1, 4, 2, 2, 4, 5, light);
        brush.box(1, 1, 2, 1, 3, 2, light);
        brush.box(1, 1, 5, 1, 3, 5, light);
    }
    if (opening(*northUp, StructureDirection::North)) {
        brush.waterBox(3, 5, 15, 4, 6, 15);
    }
    if (opening(*northUp, StructureDirection::West)) {
        brush.waterBox(0, 5, 11, 0, 6, 12);
        brush.box(1, 4, 10, 2, 4, 13, light);
        brush.box(1, 1, 10, 1, 3, 10, light);
        brush.box(1, 1, 13, 1, 3, 13, light);
    }
    if (opening(*northUp, StructureDirection::East)) {
        brush.waterBox(7, 5, 11, 7, 6, 12);
        brush.box(5, 4, 10, 6, 4, 13, light);
        brush.box(6, 1, 10, 6, 3, 10, light);
        brush.box(6, 1, 13, 6, 3, 13, light);
    }
}

void monumentDoubleRooms::doubleZ(const MonumentBrush& brush, const MonumentRoom& south) {
    const MonumentRoom* north = south.connection(ordinal(StructureDirection::North));
    if (north == nullptr) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t lamp = monumentPalette::lamp();
    if (aboveGround(south)) {
        brush.defaultFloor(0, 8, opening(*north, StructureDirection::Down));
        brush.defaultFloor(0, 0, opening(south, StructureDirection::Down));
    }
    if (up(south) == nullptr) {
        brush.boxOnFillOnly(1, 4, 1, 6, 4, 7, gray);
    }
    if (up(*north) == nullptr) {
        brush.boxOnFillOnly(1, 4, 8, 6, 4, 14, gray);
    }
    for (const auto& [y, block] :
         std::array<std::pair<std::int32_t, std::int32_t>, 3>{{{3, light}, {2, gray}, {1, light}}}) {
        brush.box(0, y, 0, 0, y, 15, block);
        brush.box(7, y, 0, 7, y, 15, block);
        brush.box(1, y, 0, 7, y, 0, block);
        brush.box(1, y, 15, 6, y, 15, block);
    }
    for (const std::int32_t z : {1, 13}) {
        brush.box(1, 1, z, 1, 1, z + 1, light);
        brush.box(6, 1, z, 6, 1, z + 1, light);
        brush.box(1, 3, z, 1, 3, z + 1, light);
        brush.box(6, 3, z, 6, 3, z + 1, light);
    }
    brush.box(2, 1, 6, 2, 3, 6, light);
    brush.box(5, 1, 6, 5, 3, 6, light);
    brush.box(2, 1, 9, 2, 3, 9, light);
    brush.box(5, 1, 9, 5, 3, 9, light);
    brush.box(3, 2, 6, 4, 2, 6, light);
    brush.box(3, 2, 9, 4, 2, 9, light);
    brush.box(2, 2, 7, 2, 2, 8, light);
    brush.box(5, 2, 7, 5, 2, 8, light);
    brush.place(lamp, 2, 2, 5);
    brush.place(lamp, 5, 2, 5);
    brush.place(lamp, 2, 2, 10);
    brush.place(lamp, 5, 2, 10);
    brush.place(light, 2, 3, 5);
    brush.place(light, 5, 3, 5);
    brush.place(light, 2, 3, 10);
    brush.place(light, 5, 3, 10);
    if (opening(south, StructureDirection::South)) {
        brush.waterBox(3, 1, 0, 4, 2, 0);
    }
    if (opening(south, StructureDirection::East)) {
        brush.waterBox(7, 1, 3, 7, 2, 4);
    }
    if (opening(south, StructureDirection::West)) {
        brush.waterBox(0, 1, 3, 0, 2, 4);
    }
    if (opening(*north, StructureDirection::North)) {
        brush.waterBox(3, 1, 15, 4, 2, 15);
    }
    if (opening(*north, StructureDirection::West)) {
        brush.waterBox(0, 1, 11, 0, 2, 12);
    }
    if (opening(*north, StructureDirection::East)) {
        brush.waterBox(7, 1, 11, 7, 2, 12);
    }
}

} // namespace basalt::server::world::structure

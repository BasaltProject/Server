#include <basalt/server/world/structure/MonumentWings.hpp>

#include <array>

namespace basalt::server::world::structure {
namespace {

/// Design 0: the long hall, its roof stepping down to the entrance and lanterns down both walls.
void wingHall(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t black = monumentPalette::baseBlack();
    const std::int32_t lamp = monumentPalette::lamp();
    for (std::int32_t step = 0; step < 4; ++step) {
        brush.box(10 - step, 3 - step, 20 - step, 12 + step, 3 - step, 20, light);
    }
    brush.box(7, 0, 6, 15, 0, 16, light);
    brush.box(6, 0, 6, 6, 3, 20, light);
    brush.box(16, 0, 6, 16, 3, 20, light);
    brush.box(7, 1, 7, 7, 1, 20, light);
    brush.box(15, 1, 7, 15, 1, 20, light);
    brush.box(7, 1, 6, 9, 3, 6, light);
    brush.box(13, 1, 6, 15, 3, 6, light);
    brush.box(8, 1, 7, 9, 1, 7, light);
    brush.box(13, 1, 7, 14, 1, 7, light);
    brush.box(9, 0, 5, 13, 0, 5, light);
    brush.box(10, 0, 7, 12, 0, 7, black);
    brush.box(8, 0, 10, 8, 0, 12, black);
    brush.box(14, 0, 10, 14, 0, 12, black);
    for (std::int32_t z = 18; z >= 7; z -= 3) {
        brush.place(lamp, 6, 3, z);
        brush.place(lamp, 16, 3, z);
    }
    brush.place(lamp, 10, 0, 10);
    brush.place(lamp, 12, 0, 10);
    brush.place(lamp, 10, 0, 12);
    brush.place(lamp, 12, 0, 12);
    brush.place(lamp, 8, 3, 6);
    brush.place(lamp, 14, 3, 6);
    for (const std::int32_t x : {4, 18}) {
        for (const std::int32_t z : {4, 18}) {
            brush.place(light, x, 2, z);
            brush.place(lamp, x, 1, z);
            brush.place(light, x, 0, z);
        }
    }
    brush.place(light, 9, 7, 20);
    brush.place(light, 13, 7, 20);
    brush.box(6, 0, 21, 7, 4, 21, light);
    brush.box(15, 0, 21, 16, 4, 21, light);
    brush.spawnElder(11, 2, 16);
}

/// Design 1: the open gallery, four lantern pillars under a flat ceiling.
void wingGallery(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t black = monumentPalette::baseBlack();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.box(9, 3, 18, 13, 3, 20, light);
    brush.box(9, 0, 18, 9, 2, 18, light);
    brush.box(13, 0, 18, 13, 2, 18, light);
    for (const std::int32_t x : {9, 13}) {
        brush.place(light, x, 6, 20);
        brush.place(lamp, x, 5, 20);
        brush.place(light, x, 4, 20);
    }
    brush.box(7, 3, 7, 15, 3, 14, light);
    for (const std::int32_t x : {10, 12}) {
        brush.box(x, 0, 10, x, 6, 10, light);
        brush.box(x, 0, 12, x, 6, 12, light);
        brush.place(lamp, x, 0, 10);
        brush.place(lamp, x, 0, 12);
        brush.place(lamp, x, 4, 10);
        brush.place(lamp, x, 4, 12);
    }
    for (const std::int32_t x : {8, 14}) {
        brush.box(x, 0, 7, x, 2, 7, light);
        brush.box(x, 0, 14, x, 2, 14, light);
    }
    brush.box(8, 3, 8, 8, 3, 13, black);
    brush.box(14, 3, 8, 14, 3, 13, black);
    brush.spawnElder(11, 5, 13);
}

} // namespace

void monumentWings::wing(const MonumentBrush& brush, std::int32_t design) {
    if (design == 0) {
        wingHall(brush);
    } else {
        wingGallery(brush);
    }
}

void monumentWings::penthouse(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t black = monumentPalette::baseBlack();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.box(2, -1, 2, 11, -1, 11, light);
    brush.box(0, -1, 0, 1, -1, 11, gray);
    brush.box(12, -1, 0, 13, -1, 11, gray);
    brush.box(2, -1, 0, 11, -1, 1, gray);
    brush.box(2, -1, 12, 11, -1, 13, gray);
    brush.box(0, 0, 0, 0, 0, 13, light);
    brush.box(13, 0, 0, 13, 0, 13, light);
    brush.box(1, 0, 0, 12, 0, 0, light);
    brush.box(1, 0, 13, 12, 0, 13, light);
    for (std::int32_t step = 2; step <= 11; step += 3) {
        brush.place(lamp, 0, 0, step);
        brush.place(lamp, 13, 0, step);
        brush.place(lamp, step, 0, 0);
    }
    brush.box(2, 0, 3, 4, 0, 9, light);
    brush.box(9, 0, 3, 11, 0, 9, light);
    brush.box(4, 0, 9, 9, 0, 11, light);
    brush.place(light, 5, 0, 8);
    brush.place(light, 8, 0, 8);
    brush.place(light, 10, 0, 10);
    brush.place(light, 3, 0, 10);
    brush.box(3, 0, 3, 3, 0, 7, black);
    brush.box(10, 0, 3, 10, 0, 7, black);
    brush.box(6, 0, 10, 7, 0, 10, black);
    for (const std::int32_t x : {3, 10}) {
        for (std::int32_t z = 2; z <= 8; z += 3) {
            brush.box(x, 0, z, x, 2, z, light);
        }
    }
    brush.box(5, 0, 10, 5, 2, 10, light);
    brush.box(8, 0, 10, 8, 2, 10, light);
    brush.box(6, -1, 7, 7, -1, 8, black);
    brush.waterBox(6, -1, 3, 7, -1, 4);
    brush.spawnElder(6, 1, 6);
}

} // namespace basalt::server::world::structure

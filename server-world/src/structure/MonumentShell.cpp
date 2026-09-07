#include <basalt/server/world/structure/MonumentShell.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::world::structure {
namespace {

void wing(const MonumentBrush& brush, bool flipped, std::int32_t xOffset) {
    if (!brush.chunkIntersects(xOffset, 0, xOffset + 23, 20)) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t dot = monumentPalette::dot();
    brush.box(xOffset, 0, 0, xOffset + 24, 0, 20, gray);
    brush.waterBox(xOffset, 1, 0, xOffset + 24, 10, 20);
    for (std::int32_t step = 0; step < 4; ++step) {
        brush.box(xOffset + step, step + 1, step, xOffset + step, step + 1, 20, light);
        brush.box(xOffset + step + 7, step + 5, step + 7, xOffset + step + 7, step + 5, 20, light);
        brush.box(xOffset + 17 - step, step + 5, step + 7, xOffset + 17 - step, step + 5, 20, light);
        brush.box(xOffset + 24 - step, step + 1, step, xOffset + 24 - step, step + 1, 20, light);
        brush.box(xOffset + step + 1, step + 1, step, xOffset + 23 - step, step + 1, step, light);
        brush.box(xOffset + step + 8, step + 5, step + 7, xOffset + 16 - step, step + 5, step + 7, light);
    }
    brush.box(xOffset + 4, 4, 4, xOffset + 6, 4, 20, gray);
    brush.box(xOffset + 7, 4, 4, xOffset + 17, 4, 6, gray);
    brush.box(xOffset + 18, 4, 4, xOffset + 20, 4, 20, gray);
    brush.box(xOffset + 11, 8, 11, xOffset + 13, 8, 20, gray);
    brush.place(dot, xOffset + 12, 9, 12);
    brush.place(dot, xOffset + 12, 9, 15);
    brush.place(dot, xOffset + 12, 9, 18);
    const std::int32_t left = xOffset + (flipped ? 19 : 5);
    const std::int32_t right = xOffset + (flipped ? 5 : 19);
    for (std::int32_t z = 20; z >= 5; z -= 3) {
        brush.place(dot, left, 5, z);
    }
    for (std::int32_t z = 19; z >= 7; z -= 3) {
        brush.place(dot, right, 5, z);
    }
    for (std::int32_t step = 0; step < 4; ++step) {
        const std::int32_t at = flipped ? xOffset + 24 - (17 - step * 3) : xOffset + 17 - step * 3;
        brush.place(dot, at, 5, 5);
    }
    brush.place(dot, right, 5, 5);
    brush.box(xOffset + 11, 1, 12, xOffset + 13, 7, 12, gray);
    brush.box(xOffset + 12, 1, 11, xOffset + 12, 7, 13, gray);
}

void entranceArches(const MonumentBrush& brush) {
    if (!brush.chunkIntersects(22, 5, 35, 17)) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.waterBox(25, 0, 0, 32, 8, 20);
    for (std::int32_t step = 0; step < 4; ++step) {
        const std::int32_t z = 5 + step * 4;
        brush.box(24, 2, z, 24, 4, z, light);
        brush.box(22, 4, z, 23, 4, z, light);
        brush.place(light, 25, 5, z);
        brush.place(light, 26, 6, z);
        brush.place(lamp, 26, 5, z);
        brush.box(33, 2, z, 33, 4, z, light);
        brush.box(34, 4, z, 35, 4, z, light);
        brush.place(light, 32, 5, z);
        brush.place(light, 31, 6, z);
        brush.place(lamp, 31, 5, z);
        brush.box(27, 6, z, 30, 6, z, gray);
    }
}

void entranceWall(const MonumentBrush& brush) {
    if (!brush.chunkIntersects(15, 20, 42, 21)) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t black = monumentPalette::baseBlack();
    brush.box(15, 0, 21, 42, 0, 21, gray);
    brush.waterBox(26, 1, 21, 31, 3, 21);
    brush.box(21, 12, 21, 36, 12, 21, gray);
    brush.box(17, 11, 21, 40, 11, 21, gray);
    brush.box(16, 10, 21, 41, 10, 21, gray);
    brush.box(15, 7, 21, 42, 9, 21, gray);
    brush.box(16, 6, 21, 41, 6, 21, gray);
    brush.box(17, 5, 21, 40, 5, 21, gray);
    brush.box(21, 4, 21, 36, 4, 21, gray);
    brush.box(22, 3, 21, 26, 3, 21, gray);
    brush.box(31, 3, 21, 35, 3, 21, gray);
    brush.box(23, 2, 21, 25, 2, 21, gray);
    brush.box(32, 2, 21, 34, 2, 21, gray);
    brush.box(28, 4, 20, 29, 4, 21, light);
    brush.place(light, 27, 3, 21);
    brush.place(light, 30, 3, 21);
    brush.place(light, 26, 2, 21);
    brush.place(light, 31, 2, 21);
    brush.place(light, 25, 1, 21);
    brush.place(light, 32, 1, 21);
    for (std::int32_t step = 0; step < 7; ++step) {
        brush.place(black, 28 - step, 6 + step, 21);
        brush.place(black, 29 + step, 6 + step, 21);
    }
    for (std::int32_t step = 0; step < 4; ++step) {
        brush.place(black, 28 - step, 9 + step, 21);
        brush.place(black, 29 + step, 9 + step, 21);
    }
    brush.place(black, 28, 12, 21);
    brush.place(black, 29, 12, 21);
    for (std::int32_t step = 0; step < 3; ++step) {
        brush.place(black, 22 - step * 2, 8, 21);
        brush.place(black, 22 - step * 2, 9, 21);
        brush.place(black, 35 + step * 2, 8, 21);
        brush.place(black, 35 + step * 2, 9, 21);
    }
    brush.waterBox(15, 13, 21, 42, 15, 21);
    brush.waterBox(15, 1, 21, 15, 6, 21);
    brush.waterBox(16, 1, 21, 16, 5, 21);
    brush.waterBox(17, 1, 21, 20, 4, 21);
    brush.waterBox(21, 1, 21, 21, 3, 21);
    brush.waterBox(22, 1, 21, 22, 2, 21);
    brush.waterBox(23, 1, 21, 24, 1, 21);
    brush.waterBox(42, 1, 21, 42, 6, 21);
    brush.waterBox(41, 1, 21, 41, 5, 21);
    brush.waterBox(37, 1, 21, 40, 4, 21);
    brush.waterBox(36, 1, 21, 36, 3, 21);
    brush.waterBox(33, 1, 21, 34, 1, 21);
    brush.waterBox(35, 1, 21, 35, 2, 21);
}

void roof(const MonumentBrush& brush) {
    if (!brush.chunkIntersects(21, 21, 36, 36)) {
        return;
    }
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t lamp = monumentPalette::lamp();
    brush.box(21, 0, 22, 36, 0, 36, gray);
    brush.waterBox(21, 1, 22, 36, 23, 36);
    for (std::int32_t step = 0; step < 4; ++step) {
        brush.box(21 + step, 13 + step, 21 + step, 36 - step, 13 + step, 21 + step, light);
        brush.box(21 + step, 13 + step, 36 - step, 36 - step, 13 + step, 36 - step, light);
        brush.box(21 + step, 13 + step, 22 + step, 21 + step, 13 + step, 35 - step, light);
        brush.box(36 - step, 13 + step, 22 + step, 36 - step, 13 + step, 35 - step, light);
    }
    brush.box(25, 16, 25, 32, 16, 32, gray);
    brush.box(25, 17, 25, 25, 19, 25, light);
    brush.box(32, 17, 25, 32, 19, 25, light);
    brush.box(25, 17, 32, 25, 19, 32, light);
    brush.box(32, 17, 32, 32, 19, 32, light);
    brush.place(light, 26, 20, 26);
    brush.place(light, 27, 21, 27);
    brush.place(lamp, 27, 20, 27);
    brush.place(light, 26, 20, 31);
    brush.place(light, 27, 21, 30);
    brush.place(lamp, 27, 20, 30);
    brush.place(light, 31, 20, 31);
    brush.place(light, 30, 21, 30);
    brush.place(lamp, 30, 20, 30);
    brush.place(light, 31, 20, 26);
    brush.place(light, 30, 21, 27);
    brush.place(lamp, 30, 20, 27);
    brush.box(28, 21, 27, 29, 21, 27, gray);
    brush.box(27, 21, 28, 27, 21, 29, gray);
    brush.box(28, 21, 30, 29, 21, 30, gray);
    brush.box(30, 21, 28, 30, 21, 29, gray);
}

void lowerWall(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t dot = monumentPalette::dot();
    if (brush.chunkIntersects(0, 21, 6, 58)) {
        brush.box(0, 0, 21, 6, 0, 57, gray);
        brush.waterBox(0, 1, 21, 6, 7, 57);
        brush.box(4, 4, 21, 6, 4, 53, gray);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(step, step + 1, 21, step, step + 1, 57 - step, light);
        }
        for (std::int32_t z = 23; z < 53; z += 3) {
            brush.place(dot, 5, 5, z);
        }
        brush.place(dot, 5, 5, 52);
        // The same four courses again: the jar lays them twice and the second pass is a no-op.
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(step, step + 1, 21, step, step + 1, 57 - step, light);
        }
        brush.box(4, 1, 52, 6, 3, 52, gray);
        brush.box(5, 1, 51, 5, 3, 53, gray);
    }
    if (brush.chunkIntersects(51, 21, 58, 58)) {
        brush.box(51, 0, 21, 57, 0, 57, gray);
        brush.waterBox(51, 1, 21, 57, 7, 57);
        brush.box(51, 4, 21, 53, 4, 53, gray);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(57 - step, step + 1, 21, 57 - step, step + 1, 57 - step, light);
        }
        for (std::int32_t z = 23; z < 53; z += 3) {
            brush.place(dot, 52, 5, z);
        }
        brush.place(dot, 52, 5, 52);
        brush.box(51, 1, 52, 53, 3, 52, gray);
        brush.box(52, 1, 51, 52, 3, 53, gray);
    }
    if (brush.chunkIntersects(0, 51, 57, 57)) {
        brush.box(7, 0, 51, 50, 0, 57, gray);
        brush.waterBox(7, 1, 51, 50, 10, 57);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(step + 1, step + 1, 57 - step, 56 - step, step + 1, 57 - step, light);
        }
    }
}

void middleWall(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t dot = monumentPalette::dot();
    if (brush.chunkIntersects(7, 21, 13, 50)) {
        brush.box(7, 0, 21, 13, 0, 50, gray);
        brush.waterBox(7, 1, 21, 13, 10, 50);
        brush.box(11, 8, 21, 13, 8, 53, gray);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(step + 7, step + 5, 21, step + 7, step + 5, 54, light);
        }
        for (std::int32_t z = 21; z <= 45; z += 3) {
            brush.place(dot, 12, 9, z);
        }
    }
    if (brush.chunkIntersects(44, 21, 50, 54)) {
        brush.box(44, 0, 21, 50, 0, 50, gray);
        brush.waterBox(44, 1, 21, 50, 10, 50);
        brush.box(44, 8, 21, 46, 8, 53, gray);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(50 - step, step + 5, 21, 50 - step, step + 5, 54, light);
        }
        for (std::int32_t z = 21; z <= 45; z += 3) {
            brush.place(dot, 45, 9, z);
        }
    }
    if (brush.chunkIntersects(8, 44, 49, 54)) {
        brush.box(14, 0, 44, 43, 0, 50, gray);
        brush.waterBox(14, 1, 44, 43, 10, 50);
        for (std::int32_t x = 12; x <= 45; x += 3) {
            brush.place(dot, x, 9, 45);
            brush.place(dot, x, 9, 52);
            if (x == 12 || x == 18 || x == 24 || x == 33 || x == 39 || x == 45) {
                brush.place(dot, x, 9, 47);
                brush.place(dot, x, 9, 50);
                brush.place(dot, x, 10, 45);
                brush.place(dot, x, 10, 46);
                brush.place(dot, x, 10, 51);
                brush.place(dot, x, 10, 52);
                brush.place(dot, x, 11, 47);
                brush.place(dot, x, 11, 50);
                brush.place(dot, x, 12, 48);
                brush.place(dot, x, 12, 49);
            }
        }
        for (std::int32_t step = 0; step < 3; ++step) {
            brush.box(8 + step, 5 + step, 54, 49 - step, 5 + step, 54, gray);
        }
        brush.box(11, 8, 54, 46, 8, 54, light);
        brush.box(14, 8, 44, 43, 8, 53, gray);
    }
}

void upperWall(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t dot = monumentPalette::dot();
    if (brush.chunkIntersects(14, 21, 20, 43)) {
        brush.box(14, 0, 21, 20, 0, 43, gray);
        brush.waterBox(14, 1, 22, 20, 14, 43);
        brush.box(18, 12, 22, 20, 12, 39, gray);
        brush.box(18, 12, 21, 20, 12, 21, light);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(step + 14, step + 9, 21, step + 14, step + 9, 43 - step, light);
        }
        for (std::int32_t z = 23; z <= 39; z += 3) {
            brush.place(dot, 19, 13, z);
        }
    }
    if (brush.chunkIntersects(37, 21, 43, 43)) {
        brush.box(37, 0, 21, 43, 0, 43, gray);
        brush.waterBox(37, 1, 22, 43, 14, 43);
        brush.box(37, 12, 22, 39, 12, 39, gray);
        brush.box(37, 12, 21, 39, 12, 21, light);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(43 - step, step + 9, 21, 43 - step, step + 9, 43 - step, light);
        }
        for (std::int32_t z = 23; z <= 39; z += 3) {
            brush.place(dot, 38, 13, z);
        }
    }
    if (brush.chunkIntersects(15, 37, 42, 43)) {
        brush.box(21, 0, 37, 36, 0, 43, gray);
        brush.waterBox(21, 1, 37, 36, 14, 43);
        brush.box(21, 12, 37, 36, 12, 39, gray);
        for (std::int32_t step = 0; step < 4; ++step) {
            brush.box(15 + step, step + 9, 43 - step, 42 - step, step + 9, 43 - step, light);
        }
        for (std::int32_t x = 21; x <= 36; x += 3) {
            brush.place(dot, x, 13, 38);
        }
    }
}

/// The seven by seven grid of legs, minus the one the entrance stands in.
void pillars(const MonumentBrush& brush) {
    const std::int32_t light = monumentPalette::baseLight();
    for (std::int32_t pillarX = 0; pillarX < 7; ++pillarX) {
        std::int32_t pillarZ = 0;
        while (pillarZ < 7) {
            if (pillarZ == 0 && pillarX == 3) {
                pillarZ = 6;
            }
            const std::int32_t bx = pillarX * 9;
            const std::int32_t bz = pillarZ * 9;
            for (std::int32_t width = 0; width < 4; ++width) {
                for (std::int32_t depth = 0; depth < 4; ++depth) {
                    brush.place(light, bx + width, 0, bz + depth);
                    brush.fillColumnDown(light, bx + width, -1, bz + depth);
                }
            }
            if (pillarX != 0 && pillarX != 6) {
                pillarZ += 6;
            } else {
                ++pillarZ;
            }
        }
    }
}

} // namespace

void monumentShell::place(const MonumentBrush& brush, std::int32_t seaLevel) {
    const std::int32_t waterHeight = std::max(seaLevel, 64) - brush.fill().worldY(0);
    brush.waterBox(0, 0, 0, 58, waterHeight, 58);
    wing(brush, false, 0);
    wing(brush, true, 33);
    entranceArches(brush);
    entranceWall(brush);
    roof(brush);
    lowerWall(brush);
    middleWall(brush);
    upperWall(brush);
    pillars(brush);
    // Five skirts of water stepping outward and upward, which is what keeps the sea bed from
    // pressing against the walls.
    for (std::int32_t step = 0; step < 5; ++step) {
        brush.waterBox(-1 - step, step * 2, -1 - step, -1 - step, 23, 58 + step);
        brush.waterBox(58 + step, step * 2, -1 - step, 58 + step, 23, 58 + step);
        brush.waterBox(-step, step * 2, -1 - step, 57 + step, 23, -1 - step);
        brush.waterBox(-step, step * 2, 58 + step, 57 + step, 23, 58 + step);
    }
}

} // namespace basalt::server::world::structure

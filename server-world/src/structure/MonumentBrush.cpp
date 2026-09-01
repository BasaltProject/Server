#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/MonumentBrush.hpp>

#include <algorithm>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

} // namespace

std::int32_t monumentPalette::baseGray() {
    static const std::int32_t stateId = state("prismarine");
    return stateId;
}

std::int32_t monumentPalette::baseLight() {
    static const std::int32_t stateId = state("prismarine_bricks");
    return stateId;
}

std::int32_t monumentPalette::baseBlack() {
    static const std::int32_t stateId = state("dark_prismarine");
    return stateId;
}

std::int32_t monumentPalette::dot() { return baseLight(); }

std::int32_t monumentPalette::lamp() {
    static const std::int32_t stateId = state("sea_lantern");
    return stateId;
}

std::int32_t monumentPalette::water() {
    static const std::int32_t stateId = state("water");
    return stateId;
}

std::int32_t monumentPalette::gold() {
    static const std::int32_t stateId = state("gold_block");
    return stateId;
}

std::int32_t monumentPalette::wetSponge() {
    static const std::int32_t stateId = state("wet_sponge");
    return stateId;
}

const std::set<std::string, std::less<>>& monumentPalette::fillKeep() {
    static const std::set<std::string, std::less<>> paths = {"ice", "packed_ice", "blue_ice", "water"};
    return paths;
}

const std::set<std::string, std::less<>>& monumentPalette::replaceable() {
    static const std::set<std::string, std::less<>> paths = {"water", "lava", "glow_lichen", "seagrass",
                                                             "tall_seagrass"};
    return paths;
}

void MonumentBrush::place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const {
    fill_->place(stateId, x, y, z);
}

std::int32_t MonumentBrush::blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return fill_->blockAt(x, y, z);
}

void MonumentBrush::box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                        std::int32_t z1, std::int32_t stateId) const {
    fill_->generateBox(x0, y0, z0, x1, y1, z1, stateId, stateId, false);
}

void MonumentBrush::waterBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                             std::int32_t z1) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                const std::int32_t existing = fill_->blockAt(x, y, z);
                if (monumentPalette::fillKeep().contains(pathOf(existing))) {
                    continue;
                }
                if (fill_->worldY(y) >= seaLevel_ && existing != monumentPalette::water()) {
                    fill_->place(registry::blocks::air().stateId, x, y, z);
                } else {
                    fill_->place(monumentPalette::water(), x, y, z);
                }
            }
        }
    }
}

void MonumentBrush::boxOnFillOnly(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                                  std::int32_t z1, std::int32_t stateId) const {
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                if (fill_->blockAt(x, y, z) == monumentPalette::water()) {
                    fill_->place(stateId, x, y, z);
                }
            }
        }
    }
}

void MonumentBrush::defaultFloor(std::int32_t xOff, std::int32_t zOff, bool downOpening) const {
    const std::int32_t gray = monumentPalette::baseGray();
    const std::int32_t light = monumentPalette::baseLight();
    if (!downOpening) {
        box(xOff, 0, zOff, xOff + 7, 0, zOff + 7, gray);
        return;
    }
    box(xOff, 0, zOff, xOff + 2, 0, zOff + 7, gray);
    box(xOff + 5, 0, zOff, xOff + 7, 0, zOff + 7, gray);
    box(xOff + 3, 0, zOff, xOff + 4, 0, zOff + 2, gray);
    box(xOff + 3, 0, zOff + 5, xOff + 4, 0, zOff + 7, gray);
    box(xOff + 3, 0, zOff + 2, xOff + 4, 0, zOff + 2, light);
    box(xOff + 3, 0, zOff + 5, xOff + 4, 0, zOff + 5, light);
    box(xOff + 2, 0, zOff + 3, xOff + 2, 0, zOff + 4, light);
    box(xOff + 5, 0, zOff + 3, xOff + 5, 0, zOff + 4, light);
}

void MonumentBrush::fillColumnDown(std::int32_t stateId, std::int32_t x, std::int32_t startY, std::int32_t z) const {
    std::int32_t y = startY;
    while (fill_->worldY(y) > monumentPalette::kBottomY + 1) {
        const std::int32_t existing = fill_->blockAt(x, y, z);
        if (!registry::blocks::isAir(existing) && !monumentPalette::replaceable().contains(pathOf(existing))) {
            break;
        }
        fill_->place(stateId, x, y, z);
        --y;
    }
}

void MonumentBrush::spawnElder(std::int32_t x, std::int32_t y, std::int32_t z) const {
    fill_->placeEntity(wire::common::Identifier::vanilla("elder_guardian"), x, y, z, wire::nbt::NbtCompound{}, 0.0);
}

bool MonumentBrush::chunkIntersects(std::int32_t x0, std::int32_t z0, std::int32_t x1, std::int32_t z1) const {
    const std::int32_t wx0 = fill_->worldX(x0, z0);
    const std::int32_t wz0 = fill_->worldZ(x0, z0);
    const std::int32_t wx1 = fill_->worldX(x1, z1);
    const std::int32_t wz1 = fill_->worldZ(x1, z1);
    return clip_.maxX >= std::min(wx0, wx1) && clip_.minX <= std::max(wx0, wx1) && clip_.maxZ >= std::min(wz0, wz1) &&
           clip_.minZ <= std::max(wz0, wz1);
}

} // namespace basalt::server::world::structure

#pragma once

#include <basalt/server/world/structure/StructurePieceFill.hpp>

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::world::structure {

/// parity: the palette every monument piece shares.
namespace monumentPalette {

[[nodiscard]] std::int32_t baseGray();
[[nodiscard]] std::int32_t baseLight();
[[nodiscard]] std::int32_t baseBlack();
/// parity: `DOT`, which is the light prismarine again under another name.
[[nodiscard]] std::int32_t dot();
[[nodiscard]] std::int32_t lamp();
[[nodiscard]] std::int32_t water();
[[nodiscard]] std::int32_t gold();
[[nodiscard]] std::int32_t wetSponge();

/// parity: `FILL_KEEP`, the blocks a water box refuses to touch.
[[nodiscard]] const std::set<std::string, std::less<>>& fillKeep();

/// parity: `StructurePiece.isReplaceableByStructures`.
[[nodiscard]] const std::set<std::string, std::less<>>& replaceable();

inline constexpr std::int32_t kBottomY = -64;

} // namespace monumentPalette

/// parity: the drawing half of `OceanMonumentPiece`.
class MonumentBrush {
public:
    MonumentBrush(const StructurePieceFill& fill, std::int32_t seaLevel, StructureBoundingBox clip)
        : fill_(&fill), seaLevel_(seaLevel), clip_(clip) {}

    [[nodiscard]] const StructurePieceFill& fill() const noexcept { return *fill_; }

    void place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] std::int32_t blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
             std::int32_t stateId) const;

    /// parity: `generateWaterBox`.
    void waterBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                  std::int32_t z1) const;

    /// parity: `generateBoxOnFillOnly`.
    void boxOnFillOnly(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                       std::int32_t z1, std::int32_t stateId) const;

    /// parity: `generateDefaultFloor`.
    void defaultFloor(std::int32_t xOff, std::int32_t zOff, bool downOpening) const;

    /// parity: `StructurePiece.fillColumnDown`.
    void fillColumnDown(std::int32_t stateId, std::int32_t x, std::int32_t startY, std::int32_t z) const;

    /// parity: `spawnElder`.
    void spawnElder(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// parity: `chunkIntersects`.
    [[nodiscard]] bool chunkIntersects(std::int32_t x0, std::int32_t z0, std::int32_t x1, std::int32_t z1) const;

private:
    const StructurePieceFill* fill_;
    std::int32_t seaLevel_;
    StructureBoundingBox clip_;
};

} // namespace basalt::server::world::structure

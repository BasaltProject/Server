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

/// parity: the drawing half of `OceanMonumentPiece`. Every monument piece writes through these, so
/// the flooding rule and the "only where the water already is" rule are stated once.
class MonumentBrush {
public:
    MonumentBrush(const StructurePieceFill& fill, std::int32_t seaLevel, StructureBoundingBox clip)
        : fill_(&fill), seaLevel_(seaLevel), clip_(clip) {}

    [[nodiscard]] const StructurePieceFill& fill() const noexcept { return *fill_; }

    void place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] std::int32_t blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
             std::int32_t stateId) const;

    /// parity: `generateWaterBox`. Ice of any kind and water already there are left alone; above sea
    /// level the box is air instead, which is what opens the monument's roof to the sky.
    void waterBox(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                  std::int32_t z1) const;

    /// parity: `generateBoxOnFillOnly`, which seals a ceiling only where the room is still flooded.
    void boxOnFillOnly(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1,
                       std::int32_t z1, std::int32_t stateId) const;

    /// parity: `generateDefaultFloor`. A room above the ground floor lays its own, holed if it opens
    /// downward.
    void defaultFloor(std::int32_t xOff, std::int32_t zOff, bool downOpening) const;

    /// parity: `StructurePiece.fillColumnDown`, which stops at the first block it may not replace.
    void fillColumnDown(std::int32_t stateId, std::int32_t x, std::int32_t startY, std::int32_t z) const;

    /// parity: `spawnElder`, which stands a healed elder guardian on the block rather than inside it.
    void spawnElder(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// parity: `chunkIntersects`, the per-slice guard that keeps a piece from drawing 58 blocks wide
    /// every chunk.
    [[nodiscard]] bool chunkIntersects(std::int32_t x0, std::int32_t z0, std::int32_t x1, std::int32_t z1) const;

private:
    const StructurePieceFill* fill_;
    std::int32_t seaLevel_;
    StructureBoundingBox clip_;
};

} // namespace basalt::server::world::structure

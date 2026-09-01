#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePieceFill.hpp>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::world::structure {

/// parity: `StrongholdPieces.StrongholdPiece.SmallDoorType`, the four ways a room opens onto the one
/// before it.
enum class StrongholdDoor : std::uint8_t { Opening, WoodDoor, Grates, IronDoor };

/// parity: the blocks every stronghold piece shares.
namespace strongholdPalette {

[[nodiscard]] std::int32_t state(std::string_view path);
[[nodiscard]] std::int32_t state(std::string_view path, const std::map<std::string, std::string>& properties);

[[nodiscard]] std::int32_t stoneBricks();
[[nodiscard]] std::int32_t cracked();
[[nodiscard]] std::int32_t mossy();
[[nodiscard]] std::int32_t infested();
[[nodiscard]] std::int32_t caveAir();
[[nodiscard]] std::int32_t smoothStoneSlab();
[[nodiscard]] std::int32_t stoneBrickSlab();
[[nodiscard]] std::int32_t cobblestone();
[[nodiscard]] std::int32_t ironDoorLower();
[[nodiscard]] std::int32_t ironDoorUpper();

[[nodiscard]] std::int32_t oakDoor(std::string_view facing, std::string_view half);
[[nodiscard]] std::int32_t ironDoor(std::string_view facing, std::string_view half);
/// An iron-bars state with only the named sides connected.
[[nodiscard]] std::int32_t bars(std::initializer_list<std::string_view> sides);
[[nodiscard]] std::int32_t stairs(std::string_view path, std::string_view facing, std::string_view half = "bottom");
[[nodiscard]] std::int32_t wallTorch(std::string_view facing);
[[nodiscard]] std::int32_t button(std::string_view facing);

} // namespace strongholdPalette

/// parity: the drawing half of `StrongholdPiece`. Everything a room writes goes through here, so the
/// two rules that decide a stronghold's random stream live in one place: the shell skips whatever is
/// already air, and skipping it also skips the draw that would have chosen its block.
class StrongholdBrush {
public:
    StrongholdBrush(const StructurePieceFill& fill, Chunk& chunk, random::RandomSource& random)
        : fill_(&fill), chunk_(&chunk), random_(&random) {}

    [[nodiscard]] const StructurePieceFill& fill() const noexcept { return *fill_; }

    [[nodiscard]] random::RandomSource& random() const noexcept { return *random_; }

    void place(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) const;

    void box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
             std::int32_t stateId) const;

    /// parity: `maybeGenerateBlock`, whose draw happens whether or not the block lands.
    void maybe(float chance, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const;

    /// parity: `generateBox` with `SmoothStoneSelector`. The edge is stone brick weathered four ways
    /// and the inside is hollowed to cave air; with `skipAir` an air block is left alone and draws
    /// nothing, which is why a stronghold that meets a cave has holes rather than a shifted stream.
    void shell(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
               bool skipAir = true) const;

    /// parity: `generateSmallDoor`.
    void door(StrongholdDoor type, std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// parity: `StructurePiece.createChest` with `reorient`, which turns the chest away from its wall.
    void createChest(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view lootTable) const;

    /// parity: the portal room's silverfish spawner. The jar writes it straight to the level, so it
    /// skips the piece's orientation and its replacement rule, and `setEntityId` draws nothing.
    void silverfishSpawner(std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    [[nodiscard]] std::int32_t smoothStone(bool isEdge) const;
    [[nodiscard]] bool inClip(std::int32_t worldX, std::int32_t worldY, std::int32_t worldZ) const;
    /// parity: `StructurePiece.reorient`, the chest turning its back on its one solid neighbour.
    [[nodiscard]] std::string reorient(std::int32_t x, std::int32_t y, std::int32_t z) const;
    void frame(std::int32_t x, std::int32_t y, std::int32_t z) const;

    const StructurePieceFill* fill_;
    Chunk* chunk_;
    random::RandomSource* random_;
};

} // namespace basalt::server::world::structure

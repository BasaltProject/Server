#pragma once

#include <basalt/server/world/structure/StructureBounds.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::world::structure {

/// parity: `net.minecraft.world.level.block.Rotation`.
enum class StructureRotation : std::uint8_t { None, Clockwise90, Clockwise180, Counterclockwise90 };

/// parity: `net.minecraft.world.level.block.Mirror`.
enum class StructureMirror : std::uint8_t { None, LeftRight, FrontBack };

/// What `/place template` takes, which is each enum's `getSerializedName`.
[[nodiscard]] std::string_view serializedName(StructureRotation rotation);
[[nodiscard]] std::string_view serializedName(StructureMirror mirror);

/// parity: `net.minecraft.core.Direction`, the six axis directions a jigsaw block can face.
enum class StructureDirection : std::uint8_t { Down, Up, North, South, West, East };

[[nodiscard]] std::int32_t dx(StructureDirection direction);
[[nodiscard]] std::int32_t dy(StructureDirection direction);
[[nodiscard]] std::int32_t dz(StructureDirection direction);

[[nodiscard]] StructurePos offsetBy(StructureDirection direction, StructurePos pos);

[[nodiscard]] bool isHorizontal(StructureDirection direction);

[[nodiscard]] StructureDirection opposite(StructureDirection direction);

/// parity: `Direction.mirror`. LeftRight swaps north and south (mirroring Z); FrontBack swaps east
/// and west (mirroring X).
[[nodiscard]] StructureDirection mirrored(StructureDirection direction, StructureMirror mirror);

/// parity: `Direction.getRotated`, how a rotation turns a horizontal direction around Y. Up and
/// down are unaffected.
[[nodiscard]] StructureDirection rotated(StructureDirection direction, StructureRotation rotation);

[[nodiscard]] std::string_view serializedName(StructureDirection direction);

[[nodiscard]] std::optional<StructureDirection> directionFromName(std::string_view name);

/// parity: `StructureTemplate.transform(BlockPos, Mirror, Rotation, BlockPos)`. Which enum constant
/// maps to which case was not obvious from the decompiled switch, so it was resolved against the
/// disassembled switch map: mirror LeftRight and FrontBack negate Z and X respectively, and each
/// rotation has its own pivot formula while None returns the possibly mirrored position unchanged.
[[nodiscard]] StructurePos transform(StructurePos pos, StructureMirror mirror, StructureRotation rotation,
                                     StructurePos pivot);

/// parity: `Rotation.getRotated`, composing one turn with another.
[[nodiscard]] StructureRotation rotated(StructureRotation rotation, StructureRotation by);

/// parity: `BlockPos.relative(direction, distance)`.
[[nodiscard]] StructurePos relative(StructurePos pos, StructureDirection direction, std::int32_t distance);

/// parity: `BlockPos.rotate`, which turns a position about the origin.
[[nodiscard]] StructurePos rotate(StructurePos pos, StructureRotation rotation);

/// parity: `StructureTemplate.getZeroPositionWithTransform`, which is where a turned template's own
/// zero lands.
[[nodiscard]] StructurePos zeroPositionWithTransform(StructurePos zeroPos, StructureMirror mirror,
                                                     StructureRotation rotation, std::int32_t width,
                                                     std::int32_t depth);

} // namespace basalt::server::world::structure

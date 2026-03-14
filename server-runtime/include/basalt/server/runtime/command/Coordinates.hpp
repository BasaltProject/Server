#pragma once

#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/CommandTree.hpp>

#include <array>
#include <cstdint>
#include <memory>

namespace basalt::server::runtime::command {

/// One coordinate of a position: absolute, or relative to whoever ran the command (`~`).
///
/// The +0.5 on a bare integer is vanilla's own `WorldCoordinate.parseDouble` centre correction:
/// `/tp 10 64 10` puts you in the middle of that block, not on its corner, and only when the number
/// has no decimal point and no `~`. A `^` here is an error: only a whole triple can be local.
struct WorldCoordinate {
    bool relative = false;
    double value = 0.0;

    [[nodiscard]] double resolve(double base) const { return relative ? base + value : value; }

    [[nodiscard]] static Parsed<WorldCoordinate> parse(StringReader& reader, bool centerCorrect);

    friend bool operator==(const WorldCoordinate&, const WorldCoordinate&) = default;
};

/// A parsed `x y z`, still unresolved: `~` needs whoever runs the command to mean anything.
///
/// A local triple (`^left ^up ^forwards`) keeps its three numbers in x, y and z.
struct Coordinates {
    WorldCoordinate x;
    WorldCoordinate y;
    WorldCoordinate z;
    bool local = false;

    /// Against a bare position, where a local triple has no facing and turns with none.
    [[nodiscard]] std::array<double, 3> resolve(const std::array<double, 3>& base) const;

    /// parity: `Coordinates.getPosition`, which a local triple reads from the source's anchor and
    /// rotation.
    [[nodiscard]] std::array<double, 3> resolve(const CommandSource& source) const;

    friend bool operator==(const Coordinates&, const Coordinates&) = default;
};

/// A parsed `x y z` of block coordinates, still unresolved.
struct BlockCoordinates {
    WorldCoordinate x;
    WorldCoordinate y;
    WorldCoordinate z;
    bool local = false;

    /// parity: `WorldCoordinates.getBlockPos`, which floors rather than truncating, so -0.5 is -1.
    [[nodiscard]] std::array<std::int32_t, 3> resolve(const std::array<double, 3>& base) const;

    [[nodiscard]] std::array<std::int32_t, 3> resolve(const CommandSource& source) const;

    friend bool operator==(const BlockCoordinates&, const BlockCoordinates&) = default;
};

/// `minecraft:column_pos`: `x z`, the pair `/forceload` works in chunk coordinates with.
struct ColumnCoordinates {
    WorldCoordinate x;
    WorldCoordinate z;

    [[nodiscard]] std::array<std::int32_t, 2> resolve(const std::array<double, 3>& base) const;

    [[nodiscard]] std::array<std::int32_t, 2> resolve(const CommandSource& source) const {
        return resolve(source.position());
    }

    friend bool operator==(const ColumnCoordinates&, const ColumnCoordinates&) = default;
};

/// `minecraft:vec3`. Centre-corrects bare integers, as vanilla's own `Vec3Argument.vec3()` does.
/// The value is a `Coordinates`.
[[nodiscard]] std::shared_ptr<const ArgumentType> vec3Argument();

/// `minecraft:vec2`: `x z`, with the height left at zero. The value is a `Coordinates`.
[[nodiscard]] std::shared_ptr<const ArgumentType> vec2Argument();

/// `minecraft:block_pos`. Never centre-corrects: a block coordinate is the block, not its middle.
/// The value is a `BlockCoordinates`.
[[nodiscard]] std::shared_ptr<const ArgumentType> blockPosArgument();

/// `minecraft:column_pos`. The value is a `ColumnCoordinates`.
[[nodiscard]] std::shared_ptr<const ArgumentType> columnPosArgument();

} // namespace basalt::server::runtime::command

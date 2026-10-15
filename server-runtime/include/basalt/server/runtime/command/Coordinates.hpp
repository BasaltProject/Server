#pragma once

#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/CommandTree.hpp>

#include <array>
#include <cstdint>
#include <memory>

namespace basalt::server::runtime::command {

struct WorldCoordinate {
    bool relative = false;
    double value = 0.0;

    [[nodiscard]] double resolve(double base) const { return relative ? base + value : value; }

    [[nodiscard]] static Parsed<WorldCoordinate> parse(StringReader& reader, bool centerCorrect);

    friend bool operator==(const WorldCoordinate&, const WorldCoordinate&) = default;
};

struct Coordinates {
    WorldCoordinate x;
    WorldCoordinate y;
    WorldCoordinate z;
    bool local = false;

    [[nodiscard]] std::array<double, 3> resolve(const std::array<double, 3>& base) const;

    /// parity: `Coordinates.getPosition`.
    [[nodiscard]] std::array<double, 3> resolve(const CommandSource& source) const;

    friend bool operator==(const Coordinates&, const Coordinates&) = default;
};

struct BlockCoordinates {
    WorldCoordinate x;
    WorldCoordinate y;
    WorldCoordinate z;
    bool local = false;

    /// parity: `WorldCoordinates.getBlockPos`.
    [[nodiscard]] std::array<std::int32_t, 3> resolve(const std::array<double, 3>& base) const;

    [[nodiscard]] std::array<std::int32_t, 3> resolve(const CommandSource& source) const;

    friend bool operator==(const BlockCoordinates&, const BlockCoordinates&) = default;
};

struct ColumnCoordinates {
    WorldCoordinate x;
    WorldCoordinate z;

    [[nodiscard]] std::array<std::int32_t, 2> resolve(const std::array<double, 3>& base) const;

    [[nodiscard]] std::array<std::int32_t, 2> resolve(const CommandSource& source) const {
        return resolve(source.position());
    }

    friend bool operator==(const ColumnCoordinates&, const ColumnCoordinates&) = default;
};

[[nodiscard]] std::shared_ptr<const ArgumentType> vec3Argument();

[[nodiscard]] std::shared_ptr<const ArgumentType> vec2Argument();

/// Never centre-corrects: a block coordinate is the block, not its middle.
[[nodiscard]] std::shared_ptr<const ArgumentType> blockPosArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> columnPosArgument();

} // namespace basalt::server::runtime::command

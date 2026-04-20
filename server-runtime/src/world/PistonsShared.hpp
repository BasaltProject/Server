#pragma once

#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::pistons::internal {

using registry::BlockFace;

/// parity: `PistonBaseBlock.TRIGGER_EXTEND` and `TRIGGER_CONTRACT`.
inline constexpr std::int32_t kExtend = 0;
inline constexpr std::int32_t kContract = 1;
/// parity: `checkIfExtend`'s second kind of pull back.
inline constexpr std::int32_t kContractSliding = 2;

inline constexpr std::string_view kExtendSound = "block.piston.extend";
inline constexpr std::string_view kContractSound = "block.piston.contract";
inline constexpr float kSoundVolume = 0.5F;
inline constexpr float kExtendPitchSpread = 0.25F;
inline constexpr float kContractPitchSpread = 0.15F;
inline constexpr float kBasePitch = 0.6F;

inline constexpr std::array<BlockFace, 6> kAllFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                                    BlockFace::South, BlockFace::West, BlockFace::East};

[[nodiscard]] inline std::string_view nameOf(BlockFace face) {
    switch (face) {
    case BlockFace::Down:
        return "down";
    case BlockFace::Up:
        return "up";
    case BlockFace::North:
        return "north";
    case BlockFace::South:
        return "south";
    case BlockFace::West:
        return "west";
    default:
        return "east";
    }
}

[[nodiscard]] inline std::optional<BlockFace> faceNamed(std::string_view name) {
    for (const BlockFace face : kAllFaces) {
        if (nameOf(face) == name) {
            return face;
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] inline std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id == nullptr ? std::string{} : std::string{id->path()};
}

[[nodiscard]] inline bool isPiston(std::string_view path) { return path == "piston" || path == "sticky_piston"; }

/// parity: the four blocks `isPushable` names one by one before it looks at anything else.
[[nodiscard]] inline bool isImmovable(std::string_view path) {
    return path == "obsidian" || path == "crying_obsidian" || path == "respawn_anchor" ||
           path == "reinforced_deepslate";
}

/// parity: `Blocks.SLIME_BLOCK` and `Blocks.HONEY_BLOCK`.
[[nodiscard]] inline bool isSticky(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path == "slime_block" || path == "honey_block";
}

/// parity: `canStickToEachOther`: slime and honey each stick to anything but each other.
[[nodiscard]] inline bool sticksTo(std::int32_t one, std::int32_t other) {
    const std::string first = pathOf(one);
    const std::string second = pathOf(other);
    if ((first == "honey_block" && second == "slime_block") || (first == "slime_block" && second == "honey_block")) {
        return false;
    }
    return isSticky(one) || isSticky(other);
}

[[nodiscard]] inline std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] inline Position step(const Position& from, BlockFace face, std::int32_t times = 1) {
    return Position{from[0] + (registry::stepX(face) * times), from[1] + (registry::stepY(face) * times),
                    from[2] + (registry::stepZ(face) * times)};
}

} // namespace basalt::server::runtime::world::pistons::internal

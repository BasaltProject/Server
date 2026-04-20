#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::registry {
enum class BlockFace : std::uint8_t { Down, Up, North, South, West, East };

inline constexpr int kBlockFaceCount = 6;

[[nodiscard]] constexpr int axisOf(BlockFace face) noexcept {
    switch (face) {
    case BlockFace::Down:
    case BlockFace::Up:
        return 1;
    case BlockFace::North:
    case BlockFace::South:
        return 2;
    default:
        return 0;
    }
}

[[nodiscard]] constexpr bool isPositive(BlockFace face) noexcept {
    return face == BlockFace::Up || face == BlockFace::South || face == BlockFace::East;
}

[[nodiscard]] constexpr BlockFace opposite(BlockFace face) noexcept {
    switch (face) {
    case BlockFace::Down:
        return BlockFace::Up;
    case BlockFace::Up:
        return BlockFace::Down;
    case BlockFace::North:
        return BlockFace::South;
    case BlockFace::South:
        return BlockFace::North;
    case BlockFace::West:
        return BlockFace::East;
    default:
        return BlockFace::West;
    }
}

[[nodiscard]] constexpr int stepX(BlockFace face) noexcept {
    return axisOf(face) == 0 ? (isPositive(face) ? 1 : -1) : 0;
}

[[nodiscard]] constexpr int stepY(BlockFace face) noexcept {
    return axisOf(face) == 1 ? (isPositive(face) ? 1 : -1) : 0;
}

[[nodiscard]] constexpr int stepZ(BlockFace face) noexcept {
    return axisOf(face) == 2 ? (isPositive(face) ? 1 : -1) : 0;
}

// parity: `BlockBehaviour.BlockStateBase.getCollisionShape`.
namespace blockShapes {
inline constexpr int kUnits = 32;
inline constexpr int kCorners = 6;

[[nodiscard]] std::int32_t dampening(std::int32_t stateId);

// parity: `BlockState.blocksMotion`, which a cobweb and a bamboo sapling answer no.
[[nodiscard]] bool blocksMotion(std::int32_t stateId);

// parity: `BlockBehaviour.Properties.replaceable`.
[[nodiscard]] bool isReplaceable(std::int32_t stateId);

// parity: `BlockState.isSolid`.
[[nodiscard]] bool isSolidState(std::int32_t stateId);

// parity: `BlockState.isSolidRender`.
[[nodiscard]] bool isSolidRender(std::int32_t stateId);

[[nodiscard]] bool isFullCube(std::int32_t stateId);

[[nodiscard]] std::span<const std::int32_t> collisionBoxes(std::int32_t stateId);

[[nodiscard]] bool isEmptyShape(std::int32_t stateId);

// parity: `BlockState.isFaceSturdy`.
[[nodiscard]] bool isFaceSturdy(std::int32_t stateId, BlockFace face);

// parity: `Block.isFaceFull`.
[[nodiscard]] bool isCollisionFaceFull(std::int32_t stateId, BlockFace face);

// parity: `MultifaceBlock.canAttachTo`.
[[nodiscard]] bool canAttachTo(std::int32_t stateId, BlockFace face);

// parity: `SupportType.CENTER`.
[[nodiscard]] bool supportsCentre(std::int32_t stateId, BlockFace face);

// parity: `SupportType.RIGID`, the ring around a face rather than its middle.
[[nodiscard]] bool supportsRigid(std::int32_t stateId, BlockFace face);

// parity: whether `BlockState.getFaceOcclusionShape(face)` fills the entire face.
[[nodiscard]] bool isFaceOcclusionFull(std::int32_t stateId, BlockFace face);

// parity: `Shapes.mergedFaceOccludes`.
[[nodiscard]] bool mergedFaceOccludes(std::int32_t sourceStateId, std::int32_t targetStateId, BlockFace face);

// parity: `Shapes.joinIsNotEmpty` over the face window, true when the state fills it.
[[nodiscard]] bool faceCovers(std::int32_t stateId, BlockFace face, std::int32_t minRow, std::int32_t maxRow,
                              std::int32_t minColumn, std::int32_t maxColumn);
} // namespace blockShapes
} // namespace basalt::server::registry

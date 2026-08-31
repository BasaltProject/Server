#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::registry {

/// The six faces, in vanilla's own `Direction` order.
enum class BlockFace : std::uint8_t { Down, Up, North, South, West, East };

inline constexpr int kBlockFaceCount = 6;

/// Which axis a face points along: 0 for x, 1 for y, 2 for z.
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

/// One step along this face, which is what every walk over neighbours needs.
[[nodiscard]] constexpr int stepX(BlockFace face) noexcept {
    return axisOf(face) == 0 ? (isPositive(face) ? 1 : -1) : 0;
}

[[nodiscard]] constexpr int stepY(BlockFace face) noexcept {
    return axisOf(face) == 1 ? (isPositive(face) ? 1 : -1) : 0;
}

[[nodiscard]] constexpr int stepZ(BlockFace face) noexcept {
    return axisOf(face) == 2 ? (isPositive(face) ? 1 : -1) : 0;
}

/// The collision shape of every block state, as the boxes vanilla's own classes report.
///
/// No general `VoxelShape`, because nothing here needs one: the three questions asked of a shape all
/// answer from boxes in thirty-seconds, which is exact for every vanilla block.
///
/// parity: `BlockBehaviour.BlockStateBase.getCollisionShape`, `isFaceSturdy`, `isSolidRender` and
/// `getLightDampening`.
namespace blockShapes {

/// A block is 32 units on a side here, the finest grid any vanilla shape needs.
inline constexpr int kUnits = 32;
/// Six numbers to a box: min x, y, z then max x, y, z.
inline constexpr int kCorners = 6;

/// How much light the block takes out of what passes through it: 0, 1 or 15.
[[nodiscard]] std::int32_t dampening(std::int32_t stateId);

/// Whether the block stops something moving through it, which is what decides whether a fluid may
/// enter it at all. parity: `BlockState.blocksMotion`, which a cobweb and a bamboo sapling answer no
/// to despite being solid.
[[nodiscard]] bool blocksMotion(std::int32_t stateId);

/// Whether placing something here replaces this block instead of going next to it.
///
/// parity: `BlockBehaviour.Properties.replaceable`, on for air, water, lava, tall grass, snow layers
/// and the rest of the undergrowth, and off for a flower or a firefly bush.
[[nodiscard]] bool isReplaceable(std::int32_t stateId);

/// parity: `BlockState.isSolid`, which is what a poured bucket asks before flooding a block.
[[nodiscard]] bool isSolidState(std::int32_t stateId);

/// Whether the state's occlusion shape fills the cube. parity: `BlockState.isSolidRender`.
[[nodiscard]] bool isSolidRender(std::int32_t stateId);

/// Whether the collision shape fills the whole cube, which is what stops a fluid dead.
[[nodiscard]] bool isFullCube(std::int32_t stateId);

/// The boxes of the collision shape, flattened six numbers to a box.
[[nodiscard]] std::span<const std::int32_t> collisionBoxes(std::int32_t stateId);

/// Whether the block has no collision at all: air, a torch, a flower.
[[nodiscard]] bool isEmptyShape(std::int32_t stateId);

/// parity: `BlockState.isFaceSturdy`, which is what decides where a fluid looks for a slope.
[[nodiscard]] bool isFaceSturdy(std::int32_t stateId, BlockFace face);

/// Whether this collision shape completely covers `face`. parity: `Block.isFaceFull`.
[[nodiscard]] bool isCollisionFaceFull(std::int32_t stateId, BlockFace face);

/// parity: `SupportType.CENTER`, the middle of a face, which is all a torch or a rail asks for.
[[nodiscard]] bool supportsCentre(std::int32_t stateId, BlockFace face);

/// parity: `SupportType.RIGID`, the ring around a face rather than its middle.
[[nodiscard]] bool supportsRigid(std::int32_t stateId, BlockFace face);

/// parity: whether `BlockState.getFaceOcclusionShape(face)` fills the entire face.
[[nodiscard]] bool isFaceOcclusionFull(std::int32_t stateId, BlockFace face);

/// Whether the two blocks together wall off the face between them, `face` pointing from the source
/// to the target.
///
/// parity: `Shapes.mergedFaceOccludes`. Only the layer of each shape touching the shared plane
/// counts, and the two are merged before asking, so two half-slabs facing each other close a gap
/// that neither closes alone.
[[nodiscard]] bool mergedFaceOccludes(std::int32_t sourceStateId, std::int32_t targetStateId, BlockFace face);

/// Whether this state's `face` fills a rectangle of it, in units, rows before columns.
///
/// parity: `Shapes.joinIsNotEmpty(test, faceShape, ONLY_FIRST)` inverted, which is how a wall asks
/// whether what stands on it covers its post or one of its sides.
[[nodiscard]] bool faceCovers(std::int32_t stateId, BlockFace face, std::int32_t minRow, std::int32_t maxRow,
                              std::int32_t minColumn, std::int32_t maxColumn);

} // namespace blockShapes

} // namespace basalt::server::registry

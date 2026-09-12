#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace basalt::server::runtime::world {

/// Which state a block goes down as: facing somewhere, on its side, upside down.
///
/// parity: each block's own `getStateForPlacement`, told apart by the properties the block really
/// has plus the lists inside for the ones that break their family's rule.
///
/// Nothing here reads the world back for support, so no block refuses a spot it cannot hold on to.
namespace blockPlacement {

/// A box, as the six numbers an obstruction test compares: min x, y, z then max x, y, z.
using Box = std::array<double, 6>;

/// parity: `Player`'s own dimensions: 0.6 wide, 1.8 tall standing and 1.5 crouching.
inline constexpr double kPlayerWidth = 0.6;
inline constexpr double kPlayerHeight = 1.8;
inline constexpr double kSneakingHeight = 1.5;

/// Everything the placement of one block reads.
///
/// `world` is optional: without it the rules that look at a neighbour are simply skipped, which is
/// what a caller that only wants the orientation of a state passes.
struct Placement {
    wire::protocol::play::Direction face = wire::protocol::play::Direction::Up;
    /// Where in the clicked block the cursor was, 0 at its floor and 1 at its ceiling.
    float clickY = 0.5F;
    float clickX = 0.5F;
    float clickZ = 0.5F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    /// The state the new block is going into, which is what a waterlogged block keeps.
    std::int32_t replacing = 0;
    const WorldState* world = nullptr;
    /// Needed only to ask the world for power, which a handful of blocks do as they land.
    std::string_view worldName;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

/// The state `block` should be placed in, or `defaultStateId` when nothing about it orients.
[[nodiscard]] std::int32_t stateFor(const wire::common::Identifier& block, std::int32_t defaultStateId,
                                    const Placement& placement, random::RandomSource& random);

/// Whether the block would be placed inside somebody.
///
/// parity: `BlockItem.canPlace` over `Level.isUnobstructed`. Touching is not overlapping, which is
/// why a block at your own feet still goes down, and an empty shape never obstructs anyone.
[[nodiscard]] bool isObstructed(std::int32_t stateId, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                std::span<const Box> boxes);

/// parity: `Player.getDimensions`, which is what the box around a player measures.
[[nodiscard]] Box playerBox(double x, double y, double z, bool sneaking);

/// parity: `Direction.fromYRot`, the quarter turn the player's yaw falls in.
[[nodiscard]] wire::protocol::play::Direction playerDirection(float yaw);

/// parity: `Direction.orderedByNearest`'s first entry: whichever face the player looks most along.
[[nodiscard]] wire::protocol::play::Direction nearestLooking(float yaw, float pitch);

} // namespace blockPlacement

/// The block that goes down when the standing one is put against a wall.
///
/// parity: `StandingAndWallBlockItem`, `SignItem` and `HangingSignItem`, three item classes that all
/// do the same thing: hold a pair of blocks and pick the wall one when the face clicked is a side.
/// Every pair in 26.2 follows the same naming, so the registry answers instead of a hardcoded table.
namespace wallVariants {

/// The wall form of `block`, or nothing when it has none.
[[nodiscard]] std::optional<wire::common::Identifier> of(const wire::common::Identifier& block);

} // namespace wallVariants

/// Where a scaffolding really lands.
///
/// parity: `ScaffoldingBlockItem.updatePlacementContext`, the one placement rule that lives in the
/// item and not in the block: clicking an existing scaffolding walks along the tower until it finds
/// room, up instead of sideways unless the player is sneaking, and refuses altogether when the
/// column has run its seven blocks out.
namespace scaffoldingPlacement {

/// parity: `ScaffoldingBlock.STABILITY_MAX_DISTANCE`.
inline constexpr std::int32_t kReach = 7;

/// The position to build at, or nothing when the click puts nothing anywhere.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
target(const WorldState& world, std::int32_t minY, std::int32_t maxY, std::int32_t clickedX, std::int32_t clickedY,
       std::int32_t clickedZ, wire::protocol::play::Direction face, float yaw, bool sneaking, bool inside);

} // namespace scaffoldingPlacement

/// parity: `MossyCarpetBlock.setPlacedBy`, which grows a second carpet over the one just placed with
/// a coin toss for each side that could stand there.
namespace mossCarpetTopper {

/// The carpet to write above these coordinates, or nothing when none grows there.
[[nodiscard]] std::optional<std::int32_t> above(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const std::function<bool()>& coin);

} // namespace mossCarpetTopper

/// Where a block's second half goes, and what state it is in.
struct SecondHalf {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;

    friend bool operator==(const SecondHalf&, const SecondHalf&) = default;
};

/// The other block a two-block one needs.
///
/// parity: `setPlacedBy` on `DoorBlock`, `BedBlock` and `DoublePlantBlock`, which is what turns the
/// one block the client asked for into the pair the game really has. Without it the half that was
/// placed has no partner, and `updateShape` takes it straight back out.
namespace doubleBlocks {

/// The second half of `stateId` placed at these coordinates, or nothing for an ordinary block.
[[nodiscard]] std::optional<SecondHalf> secondHalf(std::int32_t stateId, std::int32_t x, std::int32_t y,
                                                   std::int32_t z);

/// The state a two-block one starts as: the lower half or the foot, never the other one.
[[nodiscard]] std::int32_t firstHalf(std::int32_t stateId);

} // namespace doubleBlocks

} // namespace basalt::server::runtime::world

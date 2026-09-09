#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What a block change does to the blocks around it.
///
/// parity: `Level.updateNeighborsAt` reaching each neighbour's `neighborChanged`, where a block that
/// can no longer stand breaks and drops.
///
/// `blockSurvival::isSupported` decides what breaks. A state whose rule this build does not model is
/// left alone: a block staying up when vanilla would drop it is far cheaper than one vanishing under
/// a player.
namespace neighbourUpdates {

/// How far one change is allowed to cascade. A tower of torches comes down in one go and this is
/// never reached; the bound is there so a rule that somehow contradicts itself cannot spin.
inline constexpr std::int32_t kMaxCascade = 1024;

/// Told about every block that changed shape, so the caller can send it out.
using OnReshaped = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// Told about every block that came down, with the state it had, so the caller can drop its loot.
using OnBroken = OnReshaped;

/// What a dispenser makes of a signal reaching it.
///
/// A callback so a caller can put its own hooks in front of a dispenser; left unset, `Dispensers`
/// answers it, which is vanilla.
using DispenserNeighbourChange =
    std::function<std::optional<BlockChange>(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// The four sides and the post a wall settles on, from its neighbours and whatever covers it.
///
/// A callback so a caller can answer it differently; left unset, `Walls` answers it, which is
/// vanilla.
using WallShape = std::function<std::optional<std::map<std::string, std::string>>(
    std::int32_t stateId, std::int32_t north, std::int32_t south, std::int32_t west, std::int32_t east,
    std::int32_t above)>;

/// The way a line of dust draws itself towards what it can feed.
///
/// A callback so a caller can answer it differently; left unset, `RedstoneWire` answers it, which is
/// vanilla.
using WireReshape =
    std::function<std::optional<std::int32_t>(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int32_t stateId, std::optional<registry::BlockFace> towards)>;

/// Everything a neighbour update needs besides the blocks themselves.
///
/// A struct rather than a world object: what the cascade reads off a world is its name, where it
/// ends vertically, whether it has a sky and whether it boils water, and taking those makes the
/// whole thing testable without one.
struct Context {
    WorldState& world;
    std::string_view worldName;
    /// Where the world ends. Nothing outside these is broken, so a wrong pair only ever spares a
    /// block; the defaults are the overworld's.
    std::int32_t minY = -64;
    std::int32_t maxY = 319;
    random::RandomSource& random;
    /// parity: the `water_evaporates` dimension attribute, which is the Nether's: a wet sponge put
    /// down there dries out on the spot.
    bool evaporates = false;
    bool hasSkylight = true;

    /// Wakes the fluids around a square that just changed, and answers what moved on the spot.
    ///
    /// parity: `LiquidBlock.onPlace`, `neighborChanged` and `updateShape` all end in the same
    /// `scheduleTick`, so **any** block change is what makes water notice there is somewhere to go.
    /// A hook because the flow needs the world's game rules and this does not carry them.
    using WakeFluids = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)>;

    DispenserNeighbourChange dispenser;
    WakeFluids wakeFluids;
    WallShape wallShape;
    WireReshape wireReshape;
    reshapes::GeyserReshape geyser;
    /// The world's clock, which the game events a reactor raises are stamped with: a door swinging
    /// is a `BLOCK_OPEN`, and a vibration knows which turn it was heard on.
    std::int64_t gameTime = 0;
};

/// Breaks whatever `x`, `y`, `z` stopped holding up, and whatever that in turn stopped holding.
/// `onBroken` sees each block as it goes; `effects` collects what the blocks that reacted asked to
/// be heard.
///
/// parity: `BlockState.updateNeighbourShapes` reshapes the neighbours, never the block, so
/// `reshapeSelf` is false for a command that writes half a door and true for a hand-placed block.
void afterChange(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::vector<BlockChange>& effects, const OnBroken& onBroken, const OnReshaped& onReshaped = {},
                 bool reshapeSelf = true);

/// Joins a fence, a gate or a pane to what is now beside it, answering the new state if it moved.
///
/// parity: `CrossCollisionBlock.updateShape`, which is the half of a neighbour update that changes
/// how a block looks rather than whether it stays.
///
/// A column is more than one block, so what a bubble column did is reported through `bubbles`.
///
/// `towards` is the side the news arrives from, seen from this block, and only the dust reads it.
[[nodiscard]] std::optional<std::int32_t> reshape(const Context& context, std::int32_t x, std::int32_t y,
                                                  std::int32_t z, std::vector<BlockChange>& bubbles,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// Joins a block to what is beside it: a fence to a fence, a wall to a wall, a staircase to its
/// corner, and leaves to their trunk.
///
/// parity: this half of `updateShape` is also what vanilla's `getStateForPlacement` does for these
/// blocks, so the placement path asks for it directly instead of reshaping the block it just put
/// down: reshaping it undid every state placement had decided, a double chest included.
[[nodiscard]] std::optional<std::int32_t> connect(const Context& context, std::int32_t x, std::int32_t y,
                                                  std::int32_t z, std::int32_t stateId);

/// How far these leaves are from a trunk.
///
/// parity: `LeavesBlock.updateDistance`, six lookups and no more: a trunk counts as zero and a
/// neighbouring leaf as whatever it says it is. That works because the stored value is right to
/// begin with, which is `server::world::leafDistances`' job when the chunk arrives.
[[nodiscard]] std::int32_t leafDistance(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace neighbourUpdates

} // namespace basalt::server::runtime::world

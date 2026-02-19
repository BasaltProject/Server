#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/GolemPatterns.hpp>
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

    /// parity: `CarvedPumpkinBlock.trySpawnGolem`'s `addFreshEntity`, the half of finishing a golem
    /// that is not about blocks. A hook because making a mob belongs to the entity side.
    using SummonGolem = std::function<void(golemPatterns::Golem kind, double x, double y, double z)>;

    DispenserNeighbourChange dispenser;
    WakeFluids wakeFluids;
    WallShape wallShape;
    WireReshape wireReshape;
    reshapes::GeyserReshape geyser;
    SummonGolem summonGolem;
    /// parity: `#minecraft:copper`, the copper golem's body. A hook because the tag is read from the
    /// operator's own jar and this layer has none; unset, a copper golem is never built.
    golemPatterns::IsCopper isCopper;
    /// The world's clock, which the game events a reactor raises are stamped with: a door swinging
    /// is a `BLOCK_OPEN`, and a vibration knows which turn it was heard on.
    std::int64_t gameTime = 0;
    /// parity: the `tnt_explodes` rule, which `TntBlock.prime` reads before it lights anything. The
    /// default is the rule's own so that a site which forgets it still behaves like vanilla: passed
    /// as a hardcoded false, no signal ever lit a tnt at all.
    bool tntExplodes = true;
};

/// Breaks whatever `x`, `y`, `z` stopped holding up, and whatever that in turn stopped holding.
///
/// parity: `updateNeighbourShapes` reshapes the neighbours, never the block, so `reshapeSelf` is
/// false for a command writing half a door. `removed` has no default on purpose. `afterShapes` runs
/// after the shapes and before anything reacts: a command's own properties beat the shape update.
void afterChange(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::optional<std::int32_t> removed, std::vector<BlockChange>& effects, const OnBroken& onBroken,
                 const OnReshaped& onReshaped = {}, bool reshapeSelf = true,
                 const std::function<void()>& afterShapes = {});

/// Redraws the six squares around a change and drops whatever they no longer hold up, and nothing
/// else: nobody re-reads the power. parity: `moveBlocks` writes with `UPDATE_MOVE_BY_PISTON` and
/// without `UPDATE_NEIGHBORS`, so a lever on a block the piston took away falls but tells nobody.
void reshapeOnly(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, const OnBroken& onBroken,
                 const OnReshaped& onReshaped = {});

/// Puts back to zero the signal a target or a sculk sensor arrived carrying with no turn booked to
/// spend it. parity: `SculkSensorBlock.onPlace` and `TargetBlock.onPlace`. Separate because
/// `onPlace` runs after `overwriteWithDefinedProperties`, so a command naming `power=15` still loses
/// to it and has to ask again. Doing it twice costs nothing: the second call sees a zero.
void settleArrivedSignal(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         const OnReshaped& reshaped = {});

/// Joins a fence, a gate or a pane to what is now beside it, answering the new state if it moved.
///
/// parity: `CrossCollisionBlock.updateShape`, which is the half of a neighbour update that changes
/// how a block looks rather than whether it stays.
///
///
/// `towards` is the side the news arrives from, seen from this block, and only the dust reads it.
[[nodiscard]] std::optional<std::int32_t> reshape(const Context& context, std::int32_t x, std::int32_t y,
                                                  std::int32_t z,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// Joins a block to what is beside it: a fence to a fence, a wall to a wall, a staircase to its
/// corner, and leaves to their trunk.
///
/// parity: this half of `updateShape` is also what vanilla's `getStateForPlacement` does for these
/// blocks, so the placement path asks for it directly instead of reshaping the block it just put
/// down: reshaping it undid every state placement had decided, a double chest included.
[[nodiscard]] std::optional<std::int32_t> connect(const Context& context, std::int32_t x, std::int32_t y,
                                                  std::int32_t z, std::int32_t stateId,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// How far these leaves are from a trunk.
///
/// parity: `LeavesBlock.updateDistance`, six lookups and no more: a trunk counts as zero and a
/// neighbouring leaf as whatever it says it is. That works because the stored value is right to
/// begin with, which is `server::world::leafDistances`' job when the chunk arrives.
[[nodiscard]] std::int32_t leafDistance(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The booked turn a piece of leaves gives itself when what is around it moves: this is where the
/// distance is written, one tick after the shape update that booked it.
///
/// parity: `LeavesBlock.tick`, which is `setBlock(pos, updateDistance(state, level, pos), 3)`.
[[nodiscard]] std::optional<BlockChange> leafTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace neighbourUpdates

} // namespace basalt::server::runtime::world

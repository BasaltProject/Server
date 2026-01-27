#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// What a piston can move, and what happens when it does.
///
/// parity: `PistonStructureResolver` for the shape of the push (a line of at most twelve, plus every
/// branch a slime block drags with it) and `PistonBaseBlock.isPushable` for what refuses to move at
/// all. Obsidian stops it, a torch breaks, bedrock is unbreakable and a block that is `IGNORE` is
/// left where it stands.
///
/// The two-tick slide is a `moving_piston` carrying the state it will become, as in vanilla; what is
/// not drawn is the sliding itself, only the tick it lands on.
namespace pistons {

/// parity: `PistonStructureResolver`'s own twelve.
inline constexpr std::int32_t kPushLimit = 12;

/// parity: `PistonBaseBlock`'s own two ticks between the arm starting to move and the blocks landing,
/// counted one at a time because the slide is written out half way through the first of them.
inline constexpr std::int32_t kMoveTicks = 2;

using Position = std::array<std::int32_t, 3>;

/// Where a push ends up: what moves, in the order it has to be written, and what breaks.
struct Push {
    std::vector<Position> moved;
    std::vector<Position> destroyed;
};

/// Somebody the piston has to shove, and where they end up.
struct PlayerPosition {
    std::string uuid;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// Everything a piston needs besides the blocks: where the world ends, which world it is for the
/// redstone, and the roll behind the pitch of its own sound.
struct Context {
    WorldState& world;
    std::string_view worldName;
    std::int32_t minY = 0;
    std::int32_t maxY = 0;
    random::RandomSource& random;
    /// The world's own clock, which a game event needs: a vibration is stamped with the turn it was
    /// heard on. Only the paths that raise one read it, so `trigger` can be called without it.
    std::int64_t gameTime = 0;
    /// What a block the piston broke leaves on the floor. parity: `PistonBaseBlock.moveBlocks` runs
    /// `Block.dropResources` on everything it destroys, so a torch or a piece of dust in the way is
    /// picked up afterwards rather than gone. Measured in the bench circuit `pistonrompe`.
    std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)> onDestroyed;
};

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `PistonBaseBlock.isPushable`. Air moves, obsidian and its family never do, a block with no
/// destroy speed (bedrock) never does, and the push reaction decides the rest.
///
/// `connection` is the direction the block was reached from, and only a `PUSH_ONLY` block reads it.
[[nodiscard]] bool isPushable(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                              registry::BlockFace pushing, bool allowDestroyable, registry::BlockFace connection);

/// Works out the push a piston at these coordinates would make, or nothing when it is blocked.
///
/// parity: `PistonStructureResolver.resolve`, the line in front of the head plus every branch a
/// sticky block drags along, refusing altogether past twelve blocks.
[[nodiscard]] std::optional<Push> resolve(const Context& context, std::int32_t pistonX, std::int32_t pistonY,
                                          std::int32_t pistonZ, registry::BlockFace facing, bool extending);

/// parity: `PistonBaseBlock.checkIfExtend`: a piston extends when it is powered from anywhere but its
/// own face, and pulls back when that power goes.
///
/// It books the move and nothing else. Vanilla runs `blockEvent` here, and the event is what moves
/// the piston, once a tick and only while the world runs.
void trigger(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// The booked move starting: the arm and what it pushes turn into `moving_piston`, the body takes
/// `extended`, the landing is booked two ticks out. parity: `PistonBaseBlock.triggerEvent`. The
/// signal is read again here, so a pulse already gone by leaves the piston where it stands.
void runEvent(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t b0, std::int32_t b1,
              std::vector<BlockChange>& effects);

/// The move landing: the blocks go one along, the head appears or goes, and a sticky piston takes the
/// block in front of it back with it.
[[nodiscard]] std::vector<BlockChange> finish(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                              const std::vector<PlayerPosition>& players = {},
                                              const std::function<void(const PlayerPosition&)>& onPlayerPushed = {});

/// Moves whatever the pushed blocks sweep over.
///
/// parity: `PistonMovingBlockEntity.moveCollidedEntities`, which shifts every entity inside the area a
/// moving block covers on its way. Here that is the players and the dropped items; a player is handed
/// back through `onPlayerPushed`, because moving one means telling its client.
void dragAlong(const Context& context, const std::vector<Position>& moving, registry::BlockFace direction,
               const std::vector<PlayerPosition>& players = {},
               const std::function<void(const PlayerPosition&)>& onPlayerPushed = {});

} // namespace pistons

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
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
/// parity: `PistonStructureResolver` (a line of at most twelve, plus slime branches) and
/// `PistonBaseBlock.isPushable`. The slide is a `moving_piston` carrying its final state, as vanilla;
/// only the landing tick is drawn, not the sliding.
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
    /// What a block that has just landed tells the squares around it. parity:
    /// `PistonMovingBlockEntity.finalTick` writes with `Level.setBlock`'s flag sixty-seven, and the
    /// one in there is `UPDATE_NEIGHBORS`: the six neighbours hear it and get their shape update.
    /// Without it the tilled soil a piston covers never books the turn that gives way. Measured in
    /// `tierrapisada`.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> onLanded;
    /// Whether a square is inside the world border. parity: the very first question `isPushable`
    /// asks, before it even looks at the block. A hook because the border belongs to the world and
    /// this does not carry it; unset means "no border", which is what a test wants.
    std::function<bool(std::int32_t x, std::int32_t z)> withinBorder;
};

[[nodiscard]] const wire::common::Identifier& block();

/// The collision a `moving_piston` offers where it stands: the block it carries (or the head it
/// is), moved along by how far the slide has come, and the arm a retracting base keeps behind it.
/// parity: `PistonMovingBlockEntity.getCollisionShape`, outside a piston's own move.
[[nodiscard]] std::vector<entity::Aabb> movingCollision(const WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z);

/// What a booked half-way turn is filed under, which is the sliding block's own name so it can sit
/// beside the piston's landing turn on the same square.
[[nodiscard]] const wire::common::Identifier& slideTickType();

/// The body a piston head belongs to, if the head at these coordinates has one.
///
/// parity: `PistonHeadBlock.isFittingBase`: the right kind of piston, extended, and looking the same
/// way. `affectNeighborsAfterRemoval` uses it to break the body when the head is taken off.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
fittingBase(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t headState);

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
/// `booked` is the block that booked it: an event whose square now holds something else is dropped,
/// which is `doBlockEvent` checking `state.is(eventData.block())`.
void runEvent(const Context& context, const BlockEvent& event, std::vector<BlockChange>& effects);

/// One turn of a sliding square, which in the jar is the block entity's own ticker: the first moves
/// the progress along and the second lands it.
///
/// parity: `PistonMovingBlockEntity.tick`. Nothing is booked for it, which is why the jar's chunk
/// carries no pending tick while a piston slides.
[[nodiscard]] std::vector<BlockChange> slideTick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 const std::vector<PlayerPosition>& players,
                                                 const std::function<void(const PlayerPosition&)>& onPlayerPushed);

/// The slide ending, whichever way it went: each `moving_piston` becomes the state it carries, where it
/// stands. parity: `PistonMovingBlockEntity.finalTick`.
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

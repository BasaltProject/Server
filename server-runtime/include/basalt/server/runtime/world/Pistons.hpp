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

/// parity: `PistonStructureResolver`.
namespace pistons {

/// parity: `PistonBaseBlock`.
inline constexpr std::int32_t kMoveTicks = 2;

using Position = std::array<std::int32_t, 3>;

struct Push {
    std::vector<Position> moved;
    std::vector<Position> destroyed;
};

struct PlayerPosition {
    std::string uuid;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Context {
    WorldState& world;
    std::string_view worldName;
    std::int32_t minY = 0;
    std::int32_t maxY = 0;
    random::RandomSource& random;
    /// A vibration is stamped with the turn it was heard on; only paths that raise one read it.
    std::int64_t gameTime = 0;
    /// parity: `PistonBaseBlock.moveBlocks`.
    std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)> onDestroyed;
    /// parity: `PistonMovingBlockEntity.finalTick`.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> onLanded;
    /// parity: the first thing `isPushable` asks; unset means no border, as tests want.
    std::function<bool(std::int32_t x, std::int32_t z)> withinBorder;
};

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `PistonMovingBlockEntity.getCollisionShape`, outside a piston's own move.
[[nodiscard]] std::vector<entity::Aabb> movingCollision(const WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z);

/// Filed under the sliding block's own name so it can sit beside the landing turn on the same square.
[[nodiscard]] const wire::common::Identifier& slideTickType();

/// parity: `PistonHeadBlock.isFittingBase`.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
fittingBase(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t headState);

/// parity: `PistonBaseBlock.isPushable`.
[[nodiscard]] bool isPushable(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                              registry::BlockFace pushing, bool allowDestroyable, registry::BlockFace connection);

/// parity: `PistonStructureResolver.resolve`.
[[nodiscard]] std::optional<Push> resolve(const Context& context, std::int32_t pistonX, std::int32_t pistonY,
                                          std::int32_t pistonZ, registry::BlockFace facing, bool extending);

/// parity: `PistonBaseBlock.checkIfExtend`.
void trigger(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `PistonBaseBlock.triggerEvent`.
void runEvent(const Context& context, const BlockEvent& event, std::vector<BlockChange>& effects);

/// parity: `PistonMovingBlockEntity.tick`.
[[nodiscard]] std::vector<BlockChange> slideTick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 const std::vector<PlayerPosition>& players,
                                                 const std::function<void(const PlayerPosition&)>& onPlayerPushed);

/// parity: `PistonMovingBlockEntity.finalTick`.
[[nodiscard]] std::vector<BlockChange> finish(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                              const std::vector<PlayerPosition>& players = {},
                                              const std::function<void(const PlayerPosition&)>& onPlayerPushed = {});

/// parity: `moveCollidedEntities`.
void dragAlong(const Context& context, const std::vector<Position>& moving, registry::BlockFace direction,
               const std::vector<PlayerPosition>& players = {},
               const std::function<void(const PlayerPosition&)>& onPlayerPushed = {});

} // namespace pistons

} // namespace basalt::server::runtime::world

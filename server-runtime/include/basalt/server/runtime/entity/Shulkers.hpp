#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The shulker and its bullet.
///
/// parity: `Shulker`, a mob that never moves but by teleporting, whose lid decides its box, its armour
/// and its sound, and `ShulkerBullet`, which steps along one axis at a time towards its target.
namespace shulkers {

/// parity: `Shulker.DATA_ATTACH_FACE_ID`, `DATA_PEEK_ID` and `DATA_COLOR_ID`, after `Mob`'s own.
inline constexpr std::uint8_t kAttachFaceDataIndex = 16;
inline constexpr std::uint8_t kPeekDataIndex = 17;
inline constexpr std::uint8_t kColorDataIndex = 18;

/// parity: `Shulker.NO_COLOR`.
inline constexpr std::int8_t kNoColor = 16;

/// What a shulker keeps beyond `ServerEntity`.
struct ShulkerState {
    registry::BlockFace attachFace = registry::BlockFace::Down;
    /// parity: the raw `DATA_PEEK_ID`, 0 closed to 100 wide open.
    std::int32_t peek = 0;
    std::int8_t color = kNoColor;
    /// parity: `currentPeekAmount` and the tick before's.
    float currentPeek = 0.0F;
    float currentPeekOld = 0.0F;
    /// The health last tick, and what the hit since then came from, which is what it reacts to.
    double healthSeen = -1.0;
    std::optional<std::string> struckBy;
};

/// What a shulker bullet keeps. parity: `ShulkerBullet`'s own fields.
struct BulletState {
    std::optional<std::int32_t> target;
    std::optional<registry::BlockFace> direction;
    std::int32_t flightSteps = 0;
    double targetDeltaX = 0.0;
    double targetDeltaY = 0.0;
    double targetDeltaZ = 0.0;
};

[[nodiscard]] bool isShulker(const wire::common::Identifier& type);

[[nodiscard]] bool isBullet(const wire::common::Identifier& type);

/// The shulker's state, read once from what it was saved or summoned with.
ShulkerState& stateOf(ServerEntity& shulker);

/// Writes `AttachFace`, `Peek` and `Color` back into what the shulker saves.
void save(ServerEntity& shulker);

/// parity: `getPhysicalPeek`, the lid's travel as the box sees it.
[[nodiscard]] float physicalPeek(float amount);

/// parity: `Shulker.getProgressAabb`: the unit box grown `progress` blocks along `towards`.
[[nodiscard]] Aabb progressAabb(float size, registry::BlockFace towards, float progress, double x, double y, double z);

/// Other shulkers stand in the way of a box, as they do of anything. parity: `canBeCollidedWith`.
using Obstacles = std::span<ServerEntity* const>;

/// parity: `Shulker.canStayAt`: open there, something sturdy on that face, and room for the lid.
[[nodiscard]] bool canStayAt(const world::WorldState& world, const ServerEntity& shulker, Obstacles others,
                             const std::array<std::int32_t, 3>& at, registry::BlockFace face);

/// parity: `findAttachableSurface`, the first face in `Direction` order that holds it.
[[nodiscard]] std::optional<registry::BlockFace> attachableSurface(const world::WorldState& world,
                                                                   const ServerEntity& shulker, Obstacles others,
                                                                   const std::array<std::int32_t, 3>& at);

/// parity: `updatePeekAmount`, five hundredths a tick towards the raw peek. Answers whether it moved.
bool updatePeek(ShulkerState& state);

/// Where a teleport lands: the block and the face it holds on.
struct Landing {
    std::array<std::int32_t, 3> at;
    registry::BlockFace face;
};

/// Whether the block a shulker would hold on refuses it. parity: `#shulker_does_not_teleport_to`.
using RefusesBlock = std::function<bool(std::int32_t stateId)>;

/// parity: `teleportSomewhere`'s five tries eight blocks each way.
[[nodiscard]] std::optional<Landing> findTeleport(const world::WorldState& world, const ServerEntity& shulker,
                                                  Obstacles others, random::RandomSource& random, std::int32_t minY,
                                                  const RefusesBlock& refuses);

/// parity: `ShulkerBullet.selectNextMoveDirection`. `target` is where the target stands and how tall
/// it is, or nothing once it is gone.
void selectNextMoveDirection(const world::WorldState& world, ServerEntity& bullet, BulletState& state,
                             std::optional<std::uint8_t> avoidAxis, const std::optional<std::array<double, 4>>& target,
                             random::RandomSource& random);

/// parity: the course half of `ShulkerBullet.tick`, the steering before the move and the new
/// direction after it. The move and the hit are the caller's.
void steer(ServerEntity& bullet, BulletState& state, bool targetAlive);

void turnAfterMove(const world::WorldState& world, ServerEntity& bullet, BulletState& state,
                   const std::array<double, 4>& target, random::RandomSource& random);

} // namespace shulkers

} // namespace basalt::server::runtime::entity

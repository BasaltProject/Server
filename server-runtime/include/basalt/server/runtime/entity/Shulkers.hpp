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

/// parity: `Shulker`.
namespace shulkers {

/// parity: `Shulker.DATA_ATTACH_FACE_ID`.
inline constexpr std::uint8_t kAttachFaceDataIndex = 16;
inline constexpr std::uint8_t kPeekDataIndex = 17;
inline constexpr std::uint8_t kColorDataIndex = 18;

inline constexpr std::int8_t kNoColor = 16;

struct ShulkerState {
    registry::BlockFace attachFace = registry::BlockFace::Down;
    std::int32_t peek = 0;
    std::int8_t color = kNoColor;
    /// parity: `currentPeekAmount` and the tick before's.
    float currentPeek = 0.0F;
    float currentPeekOld = 0.0F;
    double healthSeen = -1.0;
    std::optional<std::string> struckBy;
};

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

ShulkerState& stateOf(ServerEntity& shulker);

void save(ServerEntity& shulker);

[[nodiscard]] float physicalPeek(float amount);

[[nodiscard]] Aabb progressAabb(float size, registry::BlockFace towards, float progress, double x, double y, double z);

using Obstacles = std::span<ServerEntity* const>;

[[nodiscard]] bool canStayAt(const world::WorldState& world, const ServerEntity& shulker, Obstacles others,
                             const std::array<std::int32_t, 3>& at, registry::BlockFace face);

[[nodiscard]] std::optional<registry::BlockFace> attachableSurface(const world::WorldState& world,
                                                                   const ServerEntity& shulker, Obstacles others,
                                                                   const std::array<std::int32_t, 3>& at);

bool updatePeek(ShulkerState& state);

struct Landing {
    std::array<std::int32_t, 3> at;
    registry::BlockFace face;
};

using RefusesBlock = std::function<bool(std::int32_t stateId)>;

[[nodiscard]] std::optional<Landing> findTeleport(const world::WorldState& world, const ServerEntity& shulker,
                                                  Obstacles others, random::RandomSource& random, std::int32_t minY,
                                                  const RefusesBlock& refuses);

void selectNextMoveDirection(const world::WorldState& world, ServerEntity& bullet, BulletState& state,
                             std::optional<std::uint8_t> avoidAxis, const std::optional<std::array<double, 4>>& target,
                             random::RandomSource& random);

/// parity: the course half of `ShulkerBullet.tick`.
void steer(ServerEntity& bullet, BulletState& state, bool targetAlive);

void turnAfterMove(const world::WorldState& world, ServerEntity& bullet, BulletState& state,
                   const std::array<double, 4>& target, random::RandomSource& random);

} // namespace shulkers

} // namespace basalt::server::runtime::entity

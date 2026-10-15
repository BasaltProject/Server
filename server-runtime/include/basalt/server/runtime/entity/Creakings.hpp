#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `Creaking`.
namespace creakings {

/// parity: `Creaking.CAN_MOVE`.
inline constexpr std::uint8_t kCanMoveDataIndex = 16;
inline constexpr std::uint8_t kActiveDataIndex = 17;
inline constexpr std::uint8_t kTearingDownDataIndex = 18;
inline constexpr std::uint8_t kHomeDataIndex = 19;

/// parity: the entity events a creaking sends: its swing and its shudder when hit.
inline constexpr std::int8_t kEventAttack = 4;
inline constexpr std::int8_t kEventInvulnerable = 66;
/// parity: `LivingEntity`'s puff of smoke, which a heart's new creaking comes out of.
inline constexpr std::int8_t kEventSpawnPoof = 60;

/// parity: `Creaking.TWITCH_DEATH_DURATION`.
inline constexpr std::int32_t kTearDownTicks = 45;
/// parity: `Creaking.MAX_PLAYER_STUCK_COUNTER`.
inline constexpr std::int32_t kStuckLimit = 4;
/// parity: `Creaking.ATTACK_INTERVAL`.
inline constexpr std::int32_t kAttackInterval = 40;

struct CreakingState {
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    std::int32_t lookTicker = 0;
    std::int32_t idleTicks = 0;
    std::int64_t attackCoolingUntil = 0;
    bool canMove = true;
    bool active = false;
    bool tearingDown = false;
    std::optional<std::array<std::int32_t, 3>> home;
    std::int32_t invulnerabilityTicks = 0;
    std::int32_t attackAnimationTicks = 0;
    std::int32_t playerStuckCounter = 0;
    bool hurtPending = false;
    bool hurtByPlayer = false;
    /// parity: `Node.type` of each node of the path its navigation walks, as `PathType` ids.
    std::vector<std::uint8_t> pathNodeTypes;
    /// parity: `PathNavigation.targetPos`, where its current path was drawn to.
    std::optional<std::array<std::int32_t, 3>> pathTarget;
    std::int64_t gameTime = 0;
    std::int32_t difficulty = 2;
    std::optional<std::array<double, 4>> border;
    /// parity: `PathNavigation.reachRange`.
    std::int32_t pathReach = 0;
    std::int64_t lastRecompute = 0;
    bool delayedRecompute = false;
    std::uint64_t shapeChangesSeen = 0;
};

/// parity: `CreakingHeartBlockEntity`.
struct HeartState {
    std::int64_t ticksExisted = 0;
    std::int32_t ticker = 0;
    std::int32_t emitter = 0;
    std::optional<std::array<double, 3>> emitterTarget;
    std::int32_t outputSignal = 0;
};

[[nodiscard]] bool isCreaking(const wire::common::Identifier& type);

[[nodiscard]] CreakingState& stateOf(ServerEntity& creaking);

[[nodiscard]] bool heartBound(const ServerEntity& creaking);

/// parity: `Creaking.setTransient`.
void bindTo(ServerEntity& creaking, const std::array<std::int32_t, 3>& home);

/// parity: `Creaking.hurtServer`.
bool hurtBound(ServerEntity& creaking, bool fromLivingOrProjectile, bool byPlayer);

[[nodiscard]] std::optional<wire::common::Uuid> heartCreaking(const wire::nbt::NbtCompound& heart);

void setHeartCreaking(wire::nbt::NbtCompound& heart, const std::optional<wire::common::Uuid>& creaking);

/// parity: `computeAnalogOutputSignal`: fifteen at the heart, none at thirty-two blocks.
[[nodiscard]] std::int32_t signalFor(double distance);

} // namespace creakings

} // namespace basalt::server::runtime::entity

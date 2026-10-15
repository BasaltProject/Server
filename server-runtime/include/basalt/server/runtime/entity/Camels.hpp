#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

/// parity: `Camel`, `CamelHusk` and `CamelAi`.
namespace camels {

/// parity: `Camel.DASH` and `LAST_POSE_CHANGE_TICK`, after `AbstractHorse`'s flags.
inline constexpr std::uint8_t kDashDataIndex = 19;
inline constexpr std::uint8_t kLastPoseTickDataIndex = 20;
inline constexpr std::int64_t kSitDownTicks = 40;
inline constexpr std::int64_t kStandUpTicks = 52;
/// parity: `RandomSitting(20)`, twenty seconds in a pose before the next change.
inline constexpr std::int64_t kMinimalPoseTicks = 20 * 20;

struct CamelState {
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::optional<std::int32_t> breedTarget;
    std::int32_t panicTicks = 0;
    std::int32_t lookTicker = 0;
    /// parity: `DoNothing(30, 60)`'s turns left, which hold the idle `RunOne`.
    std::int32_t idleTicks = 0;
    /// parity: `GAZE_COOLDOWN_TICKS`, `RandomLookAround`'s rest.
    std::int32_t gazeCooldown = 0;
    /// parity: `LAST_POSE_CHANGE_TICK`, negative while sitting.
    std::int64_t lastPoseTick = 0;
    double lastHealth = -1.0;
};

[[nodiscard]] bool isCamel(const wire::common::Identifier& type);

[[nodiscard]] CamelState& stateOf(ServerEntity& camel);

/// parity: `Camel.isCamelSitting`.
[[nodiscard]] bool sitting(const CamelState& state);

/// parity: `Camel.getPoseTime`.
[[nodiscard]] std::int64_t poseTime(const CamelState& state, std::int64_t gameTime);

/// parity: `Camel.refuseToMove`: sitting, or on the way up or down.
[[nodiscard]] bool refusesToMove(const CamelState& state, std::int64_t gameTime);

/// parity: `Camel.addAdditionalSaveData`'s `LastPoseTick`.
void write(const ServerEntity& camel, wire::nbt::NbtCompound& tag);

} // namespace camels

} // namespace basalt::server::runtime::entity

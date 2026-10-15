#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

/// parity: `Hoglin`, `HoglinAi` and `HoglinBase`.
namespace hoglins {

inline constexpr std::int32_t kAttackDuration = 200;
inline constexpr std::int32_t kAttackInterval = 40;
inline constexpr std::int32_t kBabyAttackInterval = 15;
inline constexpr std::int32_t kPacifyTime = 200;
inline constexpr std::array<std::int32_t, 2> kRetreatDuration{5 * 20, 20 * 20};
/// parity: `HoglinBase.ATTACK_ANIMATION_DURATION`.
inline constexpr std::int32_t kAttackAnimation = 10;

/// parity: the activities a hoglin moves between.
enum class Activity : std::uint8_t { Idle, Fight, Avoid };

struct HoglinState {
    ai::brainKit::Memory<std::int32_t> attackTarget;
    ai::brainKit::Memory<std::int32_t> avoidTarget;
    ai::brainKit::Memory<bool> pacified;
    std::optional<std::int32_t> breedTarget;
    Activity activity = Activity::Idle;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    std::int64_t attackCoolingUntil = 0;
    std::int32_t lookTicker = 0;
    std::int32_t idleTicks = 0;
    std::int32_t attackAnimation = 0;
};

[[nodiscard]] HoglinState& stateOf(ServerEntity& hoglin);

} // namespace hoglins

} // namespace basalt::server::runtime::entity

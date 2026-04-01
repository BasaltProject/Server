#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

/// The axolotl: hunting what swims, playing dead when hurt, and the player it helps.
///
/// parity: `Axolotl`, `AxolotlAi`, `PlayDead` and `ValidatePlayDead`.
namespace axolotls {

/// parity: `DATA_PLAYING_DEAD`, after the variant.
inline constexpr std::uint8_t kPlayingDeadDataIndex = 18;
inline constexpr std::int32_t kPlayDeadTime = 200;
inline constexpr std::int32_t kHuntingCooldown = 2400;
inline constexpr std::int32_t kRegenMax = 2400;
inline constexpr std::int32_t kRegenBase = 100;
inline constexpr double kPlayerRegenRange = 20.0;

enum class Activity : std::uint8_t { Idle, Fight, PlayDead };

struct AxolotlState {
    std::optional<std::int32_t> playDeadTicks;
    std::optional<std::int32_t> hurtByEntity;
    ai::brainKit::Memory<bool> huntingCooldown;
    std::optional<std::int32_t> breedTarget;
    Activity activity = Activity::Idle;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::int64_t attackCoolingUntil = 0;
    std::int32_t lookTicker = 0;
    /// parity: `PlayDead`'s run, two hundred ticks at most.
    std::int32_t playDeadRun = 0;
    /// parity: `TryFindLiquid`'s own rest between tries.
    std::int64_t nextLiquidSearch = 0;
    std::int32_t idleChoice = 0;
};

[[nodiscard]] AxolotlState& stateOf(ServerEntity& axolotl);

[[nodiscard]] bool playingDead(const ServerEntity& axolotl);

} // namespace axolotls

} // namespace basalt::server::runtime::entity

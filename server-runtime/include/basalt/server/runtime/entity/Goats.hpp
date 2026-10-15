#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/server/runtime/entity/ai/LongJump.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity {

/// parity: `Goat`.
namespace goats {

inline constexpr std::array<std::int32_t, 2> kTimeBetweenLongJumps{600, 1200};
inline constexpr std::array<std::int32_t, 2> kTimeBetweenRams{600, 6000};
inline constexpr std::array<std::int32_t, 2> kTimeBetweenRamsScreamer{100, 300};
inline constexpr std::int32_t kRamPrepareTime = 20;
inline constexpr std::int32_t kRamMinDistance = 4;
inline constexpr std::int32_t kRamMaxDistance = 7;
inline constexpr std::int32_t kLongJumpHeight = 5;
inline constexpr std::int32_t kLongJumpWidth = 5;
inline constexpr float kMaxJumpVelocityMultiplier = 3.5714288F;
/// parity: `Goat`'s synched horn flags.
inline constexpr std::uint8_t kLeftHornDataIndex = 19;
inline constexpr std::uint8_t kRightHornDataIndex = 20;

enum class Activity : std::uint8_t { Idle, LongJump, Ram };

using Block = ai::brainKit::Block;
using Vec = ai::brainKit::Vec;

struct GoatState {
    ai::longJump::State jump;
    std::optional<std::int32_t> ramCooldown;
    Activity activity = Activity::Idle;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::optional<std::int32_t> breedTarget;
    std::int32_t panicTicks = 0;
    std::int32_t lookTicker = 0;
    std::int32_t idleTicks = 0;
    /// parity: `PrepareRamNearestTarget`'s run and candidate.
    bool preparing = false;
    std::int32_t prepareRun = 0;
    std::optional<Block> ramStart;
    std::optional<Block> ramTargetBlock;
    std::optional<std::int32_t> ramEntity;
    std::optional<std::int64_t> reachedRamStart;
    /// parity: `RAM_TARGET` and `RamTarget`'s run.
    std::optional<Vec> ramTarget;
    bool ramming = false;
    std::int32_t ramRun = 0;
    Vec ramDirection{};
    bool seeded = false;
};

[[nodiscard]] GoatState& stateOf(ServerEntity& goat);

[[nodiscard]] bool isScreaming(const ServerEntity& goat);

/// parity: the two cooldowns a goat's brain saves.
void write(const ServerEntity& goat, wire::nbt::NbtCompound& tag);

} // namespace goats

} // namespace basalt::server::runtime::entity

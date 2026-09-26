#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/server/runtime/entity/ai/LongJump.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// The frog and the tadpole: the tongue, the croak, the long jumps, and the spawn a frog lays.
///
/// parity: `Frog`, `FrogAi`, `ShootTongue`, `Croak`, `TryLaySpawnOnFluidNearLand`, `Tadpole` and
/// `TadpoleAi`.
namespace frogs {

/// parity: `Frog.DATA_TONGUE_TARGET_ID`, after the variant.
inline constexpr std::uint8_t kTongueTargetDataIndex = 19;
inline constexpr std::array<std::int32_t, 2> kTimeBetweenLongJumps{100, 140};
inline constexpr std::int32_t kCroakTicks = 60;
inline constexpr float kEatingDistance = 1.75F;

enum class Activity : std::uint8_t { Idle, Swim, LaySpawn, Tongue, LongJump };

/// parity: `ShootTongue.State`.
enum class Tongue : std::uint8_t { MoveToTarget, CatchAnimation, EatAnimation, Done };

/// parity: the poses a frog shows.
enum class Pose : std::uint8_t { Standing, Croaking, UsingTongue, LongJumping };

struct FrogState {
    ai::longJump::State jump;
    Activity activity = Activity::Idle;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::optional<std::int32_t> breedTarget;
    std::int32_t panicTicks = 0;
    std::int32_t lookTicker = 0;
    bool pregnant = false;
    Pose pose = Pose::Standing;
    /// parity: `Croak`, whose counter is only reset when it starts out of liquid.
    bool croaking = false;
    std::int32_t croakCounter = 0;
    /// parity: `ShootTongue`, running from its start until done, a hundred ticks at most.
    bool tongueRunning = false;
    Tongue tongue = Tongue::Done;
    std::int64_t tongueEnd = 0;
    std::int32_t eatTimer = 0;
    std::int32_t pathCounter = 0;
    std::optional<std::int32_t> tongueTarget;
    /// parity: `UNREACHABLE_TONGUE_TARGETS`, five at most, forgotten a hundred ticks after the last.
    std::vector<wire::common::Uuid> unreachable;
    std::int64_t unreachableUntil = 0;
    /// parity: the `nextOkStartTime` of `TryFindLand` and `TryFindLandNearLiquid`.
    std::int64_t landSearch = 0;
    std::int64_t landNearLiquidSearch = 0;
    bool seeded = false;
};

/// parity: `Tadpole`'s brain, which is panic, food and swimming.
struct TadpoleState {
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::int32_t panicTicks = 0;
    std::int32_t lookTicker = 0;
};

[[nodiscard]] FrogState& stateOf(ServerEntity& frog);
[[nodiscard]] TadpoleState& tadpoleOf(ServerEntity& tadpole);

/// parity: `Frog.canEat`: the `frog_food` tag, and only the smallest of the cube mobs.
[[nodiscard]] bool canEat(const ServerEntity& mob, bool inFoodTag);

/// parity: the long jump cooldown and the pregnancy a frog's brain saves.
void write(const ServerEntity& frog, wire::nbt::NbtCompound& tag);

} // namespace frogs

} // namespace basalt::server::runtime::entity

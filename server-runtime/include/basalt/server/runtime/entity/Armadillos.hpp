#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

/// The armadillo: rolling up at danger, peeking out, unrolling, and the scute it sheds or is
/// brushed for.
///
/// parity: `Armadillo` and `ArmadilloAi`.
namespace armadillos {

/// parity: `Armadillo.ARMADILLO_STATE`, after `AgeableMob`'s own.
inline constexpr std::uint8_t kStateDataIndex = 18;
/// parity: the client's peek, `ArmadilloBallUp`'s entity event.
inline constexpr std::int8_t kPeekEvent = 64;
/// parity: `DANGER_DETECTED_RECENTLY`'s eighty ticks and the seventy-five it counts as still there.
inline constexpr std::int64_t kDangerTicks = 80;
inline constexpr std::int64_t kDangerThreshold = 75;
/// parity: `ArmadilloBallUp`'s five minutes.
inline constexpr std::int64_t kBallUpTicks = 5 * 60 * 20;

/// parity: `Armadillo.ArmadilloState`, in id order.
enum class State : std::uint8_t { Idle, Rolling, Scared, Unrolling };

/// parity: `ArmadilloState.animationDuration`.
[[nodiscard]] constexpr std::int32_t animationDuration(State state) {
    switch (state) {
    case State::Rolling:
        return 10;
    case State::Scared:
        return 50;
    case State::Unrolling:
        return 30;
    case State::Idle:
        break;
    }
    return 0;
}

struct ArmadilloState {
    State state = State::Idle;
    std::int64_t inStateTicks = 0;
    std::optional<std::int32_t> scuteTime;
    ai::brainKit::Memory<bool> danger;
    /// parity: `ArmadilloBallUp`, running until the armadillo is no longer threatened.
    bool ballUp = false;
    std::int64_t ballUpEnd = 0;
    std::int32_t nextPeek = 0;
    bool dangerWasAround = false;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::optional<std::int32_t> breedTarget;
    std::int32_t panicTicks = 0;
    std::int32_t lookTicker = 0;
    std::int32_t idleTicks = 0;
    std::int32_t gazeCooldown = 0;
};

[[nodiscard]] ArmadilloState& stateOf(ServerEntity& armadillo);

/// parity: `Armadillo.isScared`: anything but idle.
[[nodiscard]] bool isScared(const ServerEntity& armadillo);

/// parity: `Armadillo.addAdditionalSaveData`'s `state` and `scute_time`.
void write(const ServerEntity& armadillo, wire::nbt::NbtCompound& tag);

} // namespace armadillos

} // namespace basalt::server::runtime::entity

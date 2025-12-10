#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// Why a bed refused. parity: `Player.BedSleepingProblem`, by the message it puts on the screen.
enum class BedProblem : std::uint8_t { NotPossibleNow, TooFarAway, Occupied, NotHere };

/// The translation key a refusal shows.
[[nodiscard]] std::string_view translationKeyOf(BedProblem problem);

/// Sleeping in a bed: which block is the pillow, whether it can be slept in, and what it does.
///
/// parity: `BedBlock.useWithoutItem` and `ServerPlayer.startSleepInBed`, minus the two things this
/// build has no system for: no monster nearby stops anyone sleeping (there are no monsters), and a
/// bed in the Nether or the End is destroyed without the explosion that vanilla sets off, which is
/// the visible part of what it does but not all of it.
namespace beds {

/// parity: `Level.isNight`, which is when a bed will take anyone.
inline constexpr std::int64_t kNightStart = 12542;
inline constexpr std::int64_t kNightEnd = 23459;

/// parity: `ServerPlayer.bedInRange`, three blocks from the head of the bed.
inline constexpr double kRange = 3.0;

/// parity: `ServerLevel.tickTime`'s wake-up: the sleepers have to hold it for five seconds.
inline constexpr std::int32_t kSleepTicksBeforeMorning = 100;

[[nodiscard]] bool isBed(std::int32_t stateId);

[[nodiscard]] bool isOccupied(std::int32_t stateId);

[[nodiscard]] std::string facingOf(std::int32_t stateId);

[[nodiscard]] bool isHead(std::int32_t stateId);

/// The same bed with its occupied flag set, or the state unchanged if it is not a bed.
[[nodiscard]] std::int32_t withOccupied(std::int32_t stateId, bool occupied);

/// The step from the foot of a bed to its head, which is where the pillow is.
[[nodiscard]] std::array<std::int32_t, 3> headOffset(std::int32_t stateId);

/// parity: a bed only works in a dimension whose rule says so, and blows up in the others.
[[nodiscard]] bool explodesIn(config::WorldDimension dimension);

[[nodiscard]] bool isNight(std::int64_t timeOfDay);

/// parity: `ServerLevel.wakeUpAllPlayers`, which puts the clock at the next morning.
[[nodiscard]] std::int64_t nextMorning(std::int64_t timeOfDay);

/// Takes both halves of the bed away, which is what an exploding bed leaves behind.
[[nodiscard]] std::vector<BlockChange> destroy(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Every bed item, for the placement rules that need to know one when they see one.
[[nodiscard]] bool isBedItem(const wire::common::Identifier& item);

} // namespace beds

} // namespace basalt::server::runtime::world

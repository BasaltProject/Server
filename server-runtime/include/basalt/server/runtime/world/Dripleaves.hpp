#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// The big dripleaf, which is a redstone block as much as a plant: a signal holds it flat, and
/// without one it tips over in stages and comes back up on its own.
///
/// parity: `BigDripleafBlock`. `neighborChanged` resets it while there is a signal, `tick` walks the
/// stages, and every stage but `unstable` is heard by a sculk sensor as a `BLOCK_CHANGE`.
namespace dripleaves {

/// parity: `DELAY_UNTIL_NEXT_TILT_STATE`, ten ticks for each of the two on the way down and a
/// hundred lying flat before it comes back up. `none` books nothing, which is what ends the chain.
inline constexpr std::int32_t kUnstableTicks = 10;
inline constexpr std::int32_t kPartialTicks = 10;
inline constexpr std::int32_t kFullTicks = 100;

[[nodiscard]] bool isDripleaf(std::int32_t stateId);

/// Writes the tilt, books the turn that follows it and answers the change, or nothing when it was
/// already in that state.
///
/// parity: `setTiltAndScheduleTick` over `setTilt`, which writes with `UPDATE_CLIENTS` alone: a
/// dripleaf tipping tells nobody around it. What it does raise is `GameEvent.BLOCK_CHANGE`, and only
/// for the stages that `Tilt.causesVibration` says: every one but `unstable`.
[[nodiscard]] std::optional<BlockChange> setTilt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::string_view tilt, std::int64_t gameTime);

/// parity: `canEntityTilt`, which asks for both feet on the ground and a height past eleven
/// sixteenths, so brushing the stem from below is not standing on the leaf.
inline constexpr double kStandingHeight = 0.6875;

/// parity: `BigDripleafBlock.entityInside`: something standing on an upright leaf starts it tipping,
/// and only while nothing is powering it. Answers the change, or nothing when it was already tipping
/// or is being held up.
[[nodiscard]] std::optional<BlockChange> stoodOn(WorldState& world, std::string_view worldName, std::int32_t x,
                                                 std::int32_t y, std::int32_t z, double entityY, bool onGround,
                                                 std::int64_t gameTime);

/// parity: `BigDripleafBlock.neighborChanged`: a signal anywhere on it puts the leaf back up, and no
/// signal does nothing at all. This is the whole of its reaction.
[[nodiscard]] std::optional<BlockChange> react(WorldState& world, std::string_view worldName, std::int32_t x,
                                               std::int32_t y, std::int32_t z, std::int64_t gameTime);

/// parity: `BigDripleafBlock.tick`: with a signal it goes back up, and without one it takes the next
/// stage down, `full` looping back to `none`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime);

} // namespace dripleaves

} // namespace basalt::server::runtime::world

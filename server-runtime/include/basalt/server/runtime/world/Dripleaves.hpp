#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `BigDripleafBlock`.
namespace dripleaves {

/// parity: `DELAY_UNTIL_NEXT_TILT_STATE`.
inline constexpr std::int32_t kUnstableTicks = 10;
inline constexpr std::int32_t kPartialTicks = 10;
inline constexpr std::int32_t kFullTicks = 100;

[[nodiscard]] bool isDripleaf(std::int32_t stateId);

/// parity: `setTilt` writes with `UPDATE_CLIENTS` alone.
[[nodiscard]] std::optional<BlockChange> setTilt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::string_view tilt, std::int64_t gameTime);

inline constexpr double kStandingHeight = 0.6875;

/// parity: `BigDripleafBlock.entityInside`.
[[nodiscard]] std::optional<BlockChange> stoodOn(WorldState& world, std::string_view worldName, std::int32_t x,
                                                 std::int32_t y, std::int32_t z, double entityY, bool onGround,
                                                 std::int64_t gameTime);

/// parity: `BigDripleafBlock.neighborChanged`.
[[nodiscard]] std::optional<BlockChange> react(WorldState& world, std::string_view worldName, std::int32_t x,
                                               std::int32_t y, std::int32_t z, std::int64_t gameTime);

/// parity: `BigDripleafBlock.tick`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime);

} // namespace dripleaves

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `ComposterBlock`.
namespace composters {

/// parity: `ComposterBlock.READY`.
inline constexpr std::int32_t kReady = 8;
inline constexpr std::int32_t kFull = 7;

inline constexpr std::int32_t kReadyDelay = 20;

[[nodiscard]] bool isComposter(std::int32_t stateId);

/// Percent chance that this item adds a layer; parity: the `minecraft:compostable` component.
[[nodiscard]] std::optional<std::int32_t> chanceOf(const wire::common::Identifier& item);

[[nodiscard]] std::optional<std::int32_t> levelAt(const WorldState& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z);

/// parity: `ComposterBlock.addLayer`.
[[nodiscard]] std::optional<BlockChange> addItem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 const wire::common::Identifier& item, random::RandomSource& random);

/// parity: `ComposterBlock.tick`.
[[nodiscard]] std::optional<BlockChange> settle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::optional<BlockChange> empty(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace composters

} // namespace basalt::server::runtime::world

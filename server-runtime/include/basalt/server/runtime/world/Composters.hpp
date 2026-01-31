#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `ComposterBlock`. Eight levels: seven that fill by chance, and an eighth that is the bone
/// meal waiting to be taken. A comparator reads the level.
namespace composters {

/// parity: `ComposterBlock.READY`, the level a full composter sits at until somebody empties it, and
/// `MAX_LEVEL`, the last one that anything can be put into.
inline constexpr std::int32_t kReady = 8;
inline constexpr std::int32_t kFull = 7;

/// parity: the twenty ticks `addItem` books when the seventh layer goes in.
inline constexpr std::int32_t kReadyDelay = 20;

[[nodiscard]] bool isComposter(std::int32_t stateId);

/// What this item is worth to a composter, or nothing when it composts nothing. parity:
/// `ComposterBlock.COMPOSTABLES`, which the jar builds in code rather than in data.
[[nodiscard]] std::optional<float> chanceOf(const wire::common::Identifier& item);

/// How full it is, or nothing when this is not a composter.
[[nodiscard]] std::optional<std::int32_t> levelAt(const WorldState& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z);

/// Puts one item in, answering the change when the level moved and nothing when the roll failed.
///
/// parity: `ComposterBlock.addItem`. The first layer always goes in for anything compostable at all;
/// every one after it is a roll. The seventh books the turn that makes it ready.
[[nodiscard]] std::optional<BlockChange> addItem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 const wire::common::Identifier& item, random::RandomSource& random);

/// parity: `ComposterBlock.tick`: the seventh layer settling into the eighth, which is the bone meal.
[[nodiscard]] std::optional<BlockChange> settle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Empties a composter that is ready. parity: `ComposterBlock.empty`, which is level zero again; the
/// bone meal itself is the caller's to hand out.
[[nodiscard]] std::optional<BlockChange> empty(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace composters

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// What bone meal does to what it lands on. parity: `BoneMealItem.growCrop` over each block's
/// `BonemealableBlock`. Only the crops and the two stems are modelled; everything else answers
/// nothing and the bone meal is not spent, as an unbonemealable block does.
namespace boneMeal {

/// Grows what stands at these coordinates, answering the change when something grew.
[[nodiscard]] std::optional<BlockChange> apply(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               random::RandomSource& random);

/// Whether bone meal would be spent here at all. parity: `isValidBonemealTarget`, which is what
/// decides that the item goes even when the roll does nothing.
[[nodiscard]] bool grows(std::int32_t stateId);

} // namespace boneMeal

} // namespace basalt::server::runtime::world

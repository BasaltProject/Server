#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// What a grindstone makes of its input. parity: `GrindstoneMenu.computeResult`: strips all but
/// curses, or merges two of one item with their durability plus five per cent.
namespace grindstones {

/// Whether that enchantment is a curse, which is the only kind a grindstone leaves on.
using IsCurse = std::function<bool(const wire::common::Identifier& enchantment)>;

/// parity: `Enchantment.getMinCost`, what that level costs at an enchanting table, which is what a
/// grindstone hands back for taking it off.
using MinCost = std::function<std::int32_t(const wire::common::Identifier& enchantment, std::int32_t level)>;

struct Result {
    std::optional<storage::StoredItemStack> output;
    /// What both items were worth before the halving and the roll, so the caller owns its random.
    std::int32_t experience = 0;
};

[[nodiscard]] Result combine(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                             const IsCurse& isCurse = {}, const MinCost& minCost = {});

/// parity: `getExperienceAmount`: half of what the two were worth, rounded up, plus a roll of the
/// same again. `roll` is that draw, in `[0, half)`.
[[nodiscard]] std::int32_t experienceFor(std::int32_t worth, std::int32_t roll);

} // namespace grindstones

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `GrindstoneMenu.computeResult`.
namespace grindstones {

using IsCurse = std::function<bool(const wire::common::Identifier& enchantment)>;

/// parity: `Enchantment.getMinCost`.
using MinCost = std::function<std::int32_t(const wire::common::Identifier& enchantment, std::int32_t level)>;

struct Result {
    std::optional<storage::StoredItemStack> output;
    /// Worth before the halving and the roll, so the caller owns its random.
    std::int32_t experience = 0;
};

[[nodiscard]] Result combine(const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                             const IsCurse& isCurse = {}, const MinCost& minCost = {});

/// parity: `getExperienceAmount`.
[[nodiscard]] std::int32_t experienceFor(std::int32_t worth, std::int32_t roll);

} // namespace grindstones

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `EnchantmentHelper.getDamageProtection`.
namespace damageProtection {

/// In the stack's `minecraft:enchantments` order, since a float sum is not associative.
using EnchantmentLevels = std::vector<std::pair<wire::common::Identifier, std::int32_t>>;

/// Callbacks, since the damage type tag set is not ported; with either missing the answer is zero.
struct Sources {
    std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier& enchantment)> definitionOf;

    std::function<bool(const wire::common::Identifier& tag, const wire::common::Identifier& damageType)> isDamageTypeIn;
};

/// One entry per worn piece, empty when nothing is enchanted; `damageType` is null for a hit with no type.
[[nodiscard]] float of(std::span<const EnchantmentLevels> worn, const wire::common::Identifier* damageType,
                       const Sources& sources);

} // namespace damageProtection

} // namespace basalt::server::runtime::world

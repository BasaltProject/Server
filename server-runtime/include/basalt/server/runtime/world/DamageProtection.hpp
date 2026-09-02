#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// What the enchantments on a player's armour are worth against one particular hit.
///
/// parity: `EnchantmentHelper.getDamageProtection`, which sums each piece's `damage_protection`
/// effects. The requirement on each one is a damage source predicate, and every vanilla protection
/// writes it the same way: a list of damage type tags the source must or must not be in, which is
/// why a damage type tag test is enough to answer them all rather than a general predicate engine.
namespace damageProtection {

/// The enchantments on one worn piece and how deep each one is, in the order the stack's own
/// `minecraft:enchantments` component lists them: a float sum is not associative, so the order the
/// levels come in is part of the answer.
using EnchantmentLevels = std::vector<std::pair<wire::common::Identifier, std::int32_t>>;

/// What this needs from the rest of the server, as callbacks rather than the objects themselves:
/// the damage type tag set that answers the requirements is not ported yet.
///
/// Both are needed to find any protection at all, so a set with one missing answers zero rather
/// than calling nothing.
struct Sources {
    /// The enchantment's own definition, as the datapack writes it. Null for one nobody defines.
    std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier& enchantment)> definitionOf;

    /// Whether a damage type is in a damage type tag.
    std::function<bool(const wire::common::Identifier& tag, const wire::common::Identifier& damageType)> isDamageTypeIn;
};

/// The protection points the worn pieces are worth against `damageType`, which is null for a hit
/// with no type to test the requirements against.
///
/// One entry per worn piece, empty for a slot with nothing in it or with nothing enchanted on it:
/// what reads those off a stored stack is `PlayerInventory.enchantmentLevels`, which is not ported
/// yet, and a piece is nothing to this beyond the enchantments it carries.
[[nodiscard]] float of(std::span<const EnchantmentLevels> worn, const wire::common::Identifier* damageType,
                       const Sources& sources);

} // namespace damageProtection

} // namespace basalt::server::runtime::world

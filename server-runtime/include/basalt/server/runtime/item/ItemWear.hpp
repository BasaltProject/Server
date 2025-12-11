#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::item {

/// How much wear a stack has taken, and what one more use does to it.
///
/// parity: `ItemStack.isDamageableItem`, `getDamageValue` and `hurtAndBreak`. The damage lives in the
/// `minecraft:damage` component, exactly where vanilla keeps it, so an item that has never been used
/// carries no component at all and one that has carries the number a client draws its bar from.
namespace itemWear {

[[nodiscard]] bool isDamageable(const storage::StoredItemStack& stack);

[[nodiscard]] std::int32_t maxDamage(const storage::StoredItemStack& stack);

[[nodiscard]] std::int32_t damage(const storage::StoredItemStack& stack);

/// What breaking one block costs this stack, or zero for an item that does not wear that way.
[[nodiscard]] std::int32_t costPerBlock(const storage::StoredItemStack& stack);

/// What one attack costs it. Unused until there is combat, and part of the same table.
[[nodiscard]] std::int32_t costPerAttack(const storage::StoredItemStack& stack);

/// Where an enchantment gets to lower the cost of a use.
///
/// parity: vanilla applies it before the damage lands, which is why unbreaking makes a tool last
/// longer rather than repairing it afterwards.
using Reduce = std::function<std::int32_t(const storage::StoredItemStack&, std::int32_t, random::RandomSource&)>;

/// The stack after taking `amount` wear, or nothing when that broke the last of it.
[[nodiscard]] std::optional<storage::StoredItemStack> hurt(const storage::StoredItemStack& stack, std::int32_t amount,
                                                           random::RandomSource& random, const Reduce& reduce = {});

} // namespace itemWear

} // namespace basalt::server::runtime::item

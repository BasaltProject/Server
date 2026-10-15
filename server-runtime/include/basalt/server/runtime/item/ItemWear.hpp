#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::item {

namespace itemWear {

[[nodiscard]] bool isDamageable(const storage::StoredItemStack& stack);

[[nodiscard]] std::int32_t maxDamage(const storage::StoredItemStack& stack);

[[nodiscard]] std::int32_t damage(const storage::StoredItemStack& stack);

[[nodiscard]] storage::StoredItemStack withDamage(const storage::StoredItemStack& stack, std::int32_t damage);

[[nodiscard]] std::int32_t costPerBlock(const storage::StoredItemStack& stack);

[[nodiscard]] std::int32_t costPerAttack(const storage::StoredItemStack& stack);

/// parity: vanilla applies it before the damage lands, which is why unbreaking makes a tool last.
using Reduce = std::function<std::int32_t(const storage::StoredItemStack&, std::int32_t, random::RandomSource&)>;

[[nodiscard]] std::optional<storage::StoredItemStack> hurt(const storage::StoredItemStack& stack, std::int32_t amount,
                                                           random::RandomSource& random, const Reduce& reduce = {});

} // namespace itemWear

} // namespace basalt::server::runtime::item

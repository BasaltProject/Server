#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::item::itemWear {
namespace {

[[nodiscard]] wire::common::Identifier idOf(const storage::StoredItemStack& stack) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(stack.id);
    return id.has_value() ? *id : wire::common::Identifier::vanilla("air");
}

} // namespace

bool isDamageable(const storage::StoredItemStack& stack) {
    if (stack.components.contains(itemComponents::kUnbreakable)) {
        return false;
    }
    return registry::itemDurability::isDamageable(idOf(stack));
}

std::int32_t maxDamage(const storage::StoredItemStack& stack) {
    return registry::itemDurability::maxDamageOf(idOf(stack));
}

std::int32_t damage(const storage::StoredItemStack& stack) {
    const wire::nbt::NbtTag* value = stack.components.find(itemComponents::kDamage);
    const std::int32_t* stored = value == nullptr ? nullptr : value->get<std::int32_t>();
    return stored == nullptr ? 0 : std::clamp(*stored, 0, maxDamage(stack));
}

std::int32_t costPerBlock(const storage::StoredItemStack& stack) {
    const registry::itemDurability::Entry* entry = registry::itemDurability::of(idOf(stack));
    return entry == nullptr ? 0 : entry->damagePerBlock;
}

std::int32_t costPerAttack(const storage::StoredItemStack& stack) {
    const registry::itemDurability::Entry* entry = registry::itemDurability::of(idOf(stack));
    return entry == nullptr ? 0 : entry->damagePerAttack;
}

storage::StoredItemStack withDamage(const storage::StoredItemStack& stack, std::int32_t value) {
    storage::StoredItemStack changed = stack;
    const std::int32_t clamped = std::clamp(value, 0, maxDamage(stack));
    if (clamped == 0) {
        static_cast<void>(changed.components.remove(itemComponents::kDamage));
    } else {
        changed.components.putInt(itemComponents::kDamage, clamped);
    }
    return changed;
}

std::optional<storage::StoredItemStack> hurt(const storage::StoredItemStack& stack, std::int32_t amount,
                                             random::RandomSource& random, const Reduce& reduce) {
    if (amount <= 0 || !isDamageable(stack)) {
        return stack;
    }
    const std::int32_t applied = reduce ? reduce(stack, amount, random) : amount;
    if (applied <= 0) {
        return stack;
    }
    const std::int32_t max = maxDamage(stack);
    const std::int32_t newDamage = std::clamp(damage(stack) + applied, 0, max);
    // parity: `ItemStack.isBroken` is `>= max`.
    // breaks it rather than leaving a tool sitting at zero.
    if (newDamage >= max) {
        if (stack.count <= 1) {
            return std::nullopt;
        }
        storage::StoredItemStack left = stack;
        left.count = stack.count - 1;
        return left;
    }
    storage::StoredItemStack worn = stack;
    worn.components.putInt(itemComponents::kDamage, newDamage);
    return worn;
}

} // namespace basalt::server::runtime::item::itemWear

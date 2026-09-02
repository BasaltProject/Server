#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::item {

/// How much wear an enchantment takes off a use. parity: `EnchantmentHelper.processDurabilityChange`
/// over each enchantment's own `minecraft:item_damage` effect, read from the operator's own
/// definitions so unbreaking is not a special case here.
///
/// Only `remove_binomial` is implemented, the one value effect vanilla's durability enchantments
/// use; any other type leaves the amount alone rather than guessing.
class ItemDamageEffects {
public:
    /// An enchantment's definition as the jar wrote it, or null where the registry never loaded.
    using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

    /// Whether an item is in an item tag, for the effects that only apply to some of them.
    using ItemInTag = std::function<bool(std::string_view, const wire::common::Identifier&)>;

    explicit ItemDamageEffects(DefinitionOf definitionOf, ItemInTag itemInTag = {});

    [[nodiscard]] std::int32_t process(const storage::StoredItemStack& stack, std::int32_t amount,
                                       random::RandomSource& random) const;

private:
    DefinitionOf definitionOf_;
    ItemInTag itemInTag_;
};

} // namespace basalt::server::runtime::item

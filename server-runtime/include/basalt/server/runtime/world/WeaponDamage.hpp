#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// What the enchantments on a weapon add to a hit.
///
/// parity: `EnchantmentHelper.modifyDamage` over each enchantment's `minecraft:damage` effects. The
/// ones that ask something of what was hit (smite, bane of arthropods) carry a requirement on the
/// target's entity type, and they are left out until entity type tags are loaded: counting them
/// against everything would make a sword hit harder than vanilla's.
namespace weaponDamage {

/// An enchantment's definition as the jar wrote it, or nothing for one the registry does not know.
using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

[[nodiscard]] float of(const std::optional<storage::StoredItemStack>& stack, float base,
                       const DefinitionOf& definitionOf);

} // namespace weaponDamage

/// The default `minecraft:enchantable` component on vanilla 26.2 items.
namespace vanillaItemEnchantability {

[[nodiscard]] std::optional<std::int32_t> valueOf(std::string_view item);

} // namespace vanillaItemEnchantability

} // namespace basalt::server::runtime::world

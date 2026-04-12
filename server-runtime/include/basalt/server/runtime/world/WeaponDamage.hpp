#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// What the enchantments on a weapon add to a hit. parity: `EnchantmentHelper.modifyDamage` over
/// each `minecraft:damage` effect. Effects requiring a target entity type (smite, bane of
/// arthropods) are left out until entity type tags are loaded, rather than applied to everything.
namespace weaponDamage {

/// An enchantment's definition as the jar wrote it, or nothing for one the registry does not know.
using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

/// What was hit, as the `entity_properties` requirements of impaling, smite and bane read it.
struct Victim {
    wire::common::Identifier type;
    /// Whether that type is in an entity type tag.
    std::function<bool(const wire::common::Identifier& tag)> inTag;
};

[[nodiscard]] float of(const std::optional<storage::StoredItemStack>& stack, float base,
                       const DefinitionOf& definitionOf, const Victim* victim = nullptr);

/// parity: `EnchantmentHelper.runIterationOnItem` over one value effect component, such as
/// `trident_return_acceleration` or `trident_spin_attack_strength`, starting from `base`.
[[nodiscard]] float valueEffect(const storage::StoredItemStack& stack, std::string_view component, float base,
                                const DefinitionOf& definitionOf);

/// parity: `EnchantmentHelper.pickHighestLevel` over a list-per-level component, `trident_sound`.
[[nodiscard]] std::optional<std::string>
highestLevelEntry(const storage::StoredItemStack& stack, std::string_view component, const DefinitionOf& definitionOf);

} // namespace weaponDamage

/// The default `minecraft:enchantable` component on vanilla 26.2 items.
namespace vanillaItemEnchantability {

[[nodiscard]] std::optional<std::int32_t> valueOf(std::string_view item);

} // namespace vanillaItemEnchantability

} // namespace basalt::server::runtime::world

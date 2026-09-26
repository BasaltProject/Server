#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {

class PlayerSession;

/// A player's attributes: a base plus the modifiers of the six equipment slots and the effects.
/// parity: `LivingEntity.collectEquipmentChanges`, `MobEffect.addAttributeModifiers` and
/// `AttributeInstance.calculateValue`.
namespace playerAttributes {

using Modifier = wire::protocol::play::UpdateAttributesPacket::Modifier;
using Operation = wire::protocol::play::UpdateAttributesPacket::Operation;

/// An enchantment's definition as the jar wrote it, or null where the registry never loaded.
using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

/// Modifiers by attribute, the attribute named in full (`minecraft:armor`).
using Modifiers = std::map<std::string, std::vector<Modifier>, std::less<>>;

/// parity: `ItemStack.forEachModifier(slot)`: the `attribute_modifiers` component or the item's
/// default, then what its enchantments give in that slot. `slot` is `EquipmentSlot`'s name.
void addStackModifiers(const storage::StoredItemStack& stack, std::string_view slot, const DefinitionOf& definitionOf,
                       Modifiers& into);

/// Every modifier on this player right now: the unbroken stacks in hand and worn, then the effects.
[[nodiscard]] Modifiers modifiersOf(const PlayerSession& player, const DefinitionOf& definitionOf);

/// parity: `Player.createAttributes`' defaults, or what a plugin set instead.
[[nodiscard]] double baseOf(const PlayerSession& player, std::string_view attribute);

/// parity: `AttributeInstance.calculateValue`, then `RangedAttribute.sanitizeValue`.
[[nodiscard]] double calculate(std::string_view attribute, double base, const std::vector<Modifier>& modifiers);

[[nodiscard]] double valueOf(const PlayerSession& player, std::string_view attribute, const DefinitionOf& definitionOf);

} // namespace playerAttributes
} // namespace basalt::server::runtime::player

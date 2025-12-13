#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world::enchantmentAttributes {

/// The attribute modifiers an enchanted item gives whoever holds it.
///
/// The client reads mining speed off its own attribute map, which only the server fills: efficiency
/// felt like nothing at all because nobody ever sent it. parity: `EnchantmentHelper.forEachModifier`
/// plus `LivingEntity`, which adds the modifiers of what is equipped and syncs the attribute.

/// An enchantment's definition as the jar wrote it, or null where the registry never loaded.
using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

/// Every attribute a held stack modifies, as the snapshots the update packet carries.
///
/// Every attribute this build can modify is in the answer every time, so one that stops applying
/// goes back to its base rather than keeping the last modifier the client was told about.
[[nodiscard]] std::vector<wire::protocol::play::UpdateAttributesPacket::AttributeSnapshot>
snapshotsFor(const std::optional<storage::StoredItemStack>& stack, const DefinitionOf& definitionOf);

} // namespace basalt::server::runtime::world::enchantmentAttributes

#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::item::itemComponentCodecs {

/// The two forms of an item component, and the translation between them: the persistent one
/// `/give` writes and `players.db` keeps, and the network one a client reads.
///
/// Enchantments are the only ones needing the registry, since they travel as ids, so both
/// directions take a resolver that may answer nothing.

/// Enchantment name to registry index, for the stacks that carry levels.
using EnchantmentId = std::function<std::optional<std::int32_t>(const wire::common::Identifier&)>;

/// The reverse, for a stack a client sent back.
using EnchantmentName = std::function<const wire::common::Identifier*(std::int32_t)>;

/// The components with a network codec of their own here. See also `ItemComponentPayloads`.
[[nodiscard]] const std::set<std::string, std::less<>>& typed();

/// Every component a command may set, which is everything the registry knows.
///
/// A component with no network codec on either side is still stored and persisted, which is why the
/// command takes it; what it cannot do is reach the client, and `notDrawn` says which.
[[nodiscard]] bool exists(std::string_view name);

/// Whether a stack carrying `name` looks right on a client, or loses it on the way out.
[[nodiscard]] bool isSent(std::string_view name);

/// The components of `components` no client will see, by name and in the order they were written.
[[nodiscard]] std::vector<std::string> notDrawn(const wire::nbt::NbtCompound& components);

/// One stored component in the form the wire wants, or nothing when its value does not fit the
/// component (a `damage` that is not a number, an enchantment nobody can resolve).
[[nodiscard]] std::optional<wire::protocol::play::ItemComponent>
toWire(std::string_view name, const wire::nbt::NbtTag& value, const EnchantmentId& enchantmentId);

/// Every stored component of a stack in wire order, skipping the ones that do not resolve.
[[nodiscard]] std::vector<wire::protocol::play::ItemComponent> toWire(const wire::nbt::NbtCompound& components,
                                                                      const EnchantmentId& enchantmentId);

/// One component off the wire in the form this server stores, or nothing for one it does not model:
/// a client can only echo back what it was sent, so dropping those loses nothing it was given.
[[nodiscard]] std::optional<std::pair<std::string, wire::nbt::NbtTag>>
toStored(const wire::protocol::play::ItemComponent& component, const EnchantmentName& enchantmentName);

/// The reverse, for a stack a client sent back.
[[nodiscard]] wire::nbt::NbtCompound toStored(const std::vector<wire::protocol::play::ItemComponent>& components,
                                              const EnchantmentName& enchantmentName);

} // namespace basalt::server::runtime::item::itemComponentCodecs

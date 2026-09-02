#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::item::itemComponentPayloads {

/// The components whose network form is written straight into bytes rather than modelled as a type.
///
/// They ride out as `ItemComponent::Raw` because the payload is already what a client expects, and
/// a class each would buy nothing: nothing here reads them back, since a stack comes off the wire
/// in its persistent form.
///
/// A component with no case here is not refused. It is stored, persisted and reported by name, the
/// same bargain `/summon` strikes with the NBT keys it does not model.

/// Every component this can put on the wire.
[[nodiscard]] const std::set<std::string, std::less<>>& supported();

/// `value` in its network form, or nothing when the value does not fit the component's shape.
[[nodiscard]] std::optional<wire::protocol::play::ItemComponent> encode(std::string_view name,
                                                                        const wire::nbt::NbtTag& value);

} // namespace basalt::server::runtime::item::itemComponentPayloads

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

namespace playerAttributes {

using Modifier = wire::protocol::play::UpdateAttributesPacket::Modifier;
using Operation = wire::protocol::play::UpdateAttributesPacket::Operation;

using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

using Modifiers = std::map<std::string, std::vector<Modifier>, std::less<>>;

void addStackModifiers(const storage::StoredItemStack& stack, std::string_view slot, const DefinitionOf& definitionOf,
                       Modifiers& into);

[[nodiscard]] Modifiers modifiersOf(const PlayerSession& player, const DefinitionOf& definitionOf);

[[nodiscard]] double baseOf(const PlayerSession& player, std::string_view attribute);

[[nodiscard]] double calculate(std::string_view attribute, double base, const std::vector<Modifier>& modifiers);

[[nodiscard]] double valueOf(const PlayerSession& player, std::string_view attribute, const DefinitionOf& definitionOf);

} // namespace playerAttributes
} // namespace basalt::server::runtime::player

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

using EnchantmentId = std::function<std::optional<std::int32_t>(const wire::common::Identifier&)>;

using EnchantmentName = std::function<const wire::common::Identifier*(std::int32_t)>;

[[nodiscard]] const std::set<std::string, std::less<>>& typed();

[[nodiscard]] bool exists(std::string_view name);

[[nodiscard]] bool isSent(std::string_view name);

[[nodiscard]] std::vector<std::string> notDrawn(const wire::nbt::NbtCompound& components);

[[nodiscard]] std::optional<wire::protocol::play::ItemComponent>
toWire(std::string_view name, const wire::nbt::NbtTag& value, const EnchantmentId& enchantmentId);

/// Every stored component of a stack in wire order, skipping the ones that do not resolve.
[[nodiscard]] std::vector<wire::protocol::play::ItemComponent> toWire(const wire::nbt::NbtCompound& components,
                                                                      const EnchantmentId& enchantmentId);

[[nodiscard]] std::optional<std::pair<std::string, wire::nbt::NbtTag>>
toStored(const wire::protocol::play::ItemComponent& component, const EnchantmentName& enchantmentName);

[[nodiscard]] wire::nbt::NbtCompound toStored(const std::vector<wire::protocol::play::ItemComponent>& components,
                                              const EnchantmentName& enchantmentName);

} // namespace basalt::server::runtime::item::itemComponentCodecs

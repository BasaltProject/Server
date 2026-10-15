#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::item::itemComponentPayloads {

[[nodiscard]] const std::set<std::string, std::less<>>& supported();

[[nodiscard]] std::optional<wire::protocol::play::ItemComponent> encode(std::string_view name,
                                                                        const wire::nbt::NbtTag& value);

} // namespace basalt::server::runtime::item::itemComponentPayloads

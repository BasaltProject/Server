#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::plugins::dataFix {

/// The data version this server writes.
[[nodiscard]] std::int32_t currentVersion();

/// Brings a chunk, block_entity, entity or item document up to `currentVersion`; nothing for other kinds or dropped.
[[nodiscard]] std::optional<wire::nbt::NbtCompound> fixDocument(std::string_view kind, wire::nbt::NbtCompound document,
                                                                std::int32_t fromVersion);

/// The same for a `block_state` (`minecraft:oak_stairs[facing=east]`), an `item` id or a `biome` id.
[[nodiscard]] std::optional<std::string> fixName(std::string_view kind, std::string_view text,
                                                 std::int32_t fromVersion);

} // namespace basalt::server::runtime::plugins::dataFix

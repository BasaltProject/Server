#pragma once

#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world::droppedItemNbt {

/// parity: `ItemEntity`'s own save.
[[nodiscard]] wire::nbt::NbtCompound encode(const DroppedItem& item);

[[nodiscard]] bool isItem(const wire::nbt::NbtCompound& tag);

[[nodiscard]] std::optional<DroppedItem> decode(const wire::nbt::NbtCompound& tag,
                                                const std::function<std::int32_t()>& allocateEntityId);

} // namespace basalt::server::runtime::world::droppedItemNbt

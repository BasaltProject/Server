#pragma once

#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world::droppedItemNbt {

/// parity: `ItemEntity`'s own save, which is what keeps what is lying on the floor across a save,
/// a chunk unload or a restart. Without it a farm loses everything the moment nobody is looking.
///
/// The fields are vanilla's: `Pos`, `Motion`, `Age`, `PickupDelay`, `Health` and the `Item` stack.
[[nodiscard]] wire::nbt::NbtCompound encode(const DroppedItem& item);

/// Whether this saved entity is an item lying on the floor.
[[nodiscard]] bool isItem(const wire::nbt::NbtCompound& tag);

/// The item that tag describes, with a fresh runtime id, or nothing when it is not one.
[[nodiscard]] std::optional<DroppedItem> decode(const wire::nbt::NbtCompound& tag,
                                                const std::function<std::int32_t()>& allocateEntityId);

} // namespace basalt::server::runtime::world::droppedItemNbt

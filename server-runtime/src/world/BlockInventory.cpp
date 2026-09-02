#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <algorithm>
#include <expected>
#include <string>

namespace basalt::server::runtime::world::blockInventory {
namespace {

constexpr std::string_view kItems = "Items";

} // namespace

Slots read(const wire::nbt::NbtCompound& data, std::int32_t size) {
    Slots slots(static_cast<std::size_t>(std::max(size, 0)));
    const wire::nbt::NbtList* items = data.getList(kItems);
    if (items == nullptr) {
        return slots;
    }
    for (const wire::nbt::NbtTag& entry : *items) {
        const wire::nbt::NbtCompound* compound = entry.get<wire::nbt::NbtCompound>();
        if (compound == nullptr) {
            continue;
        }
        const std::int32_t slot = compound->getByte("Slot", -1);
        if (slot < 0 || slot >= size) {
            continue;
        }
        const std::string_view id = compound->getString("id");
        if (id.empty()) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
            wire::common::Identifier::parse(std::string{id});
        if (!item.has_value()) {
            continue;
        }
        slots[static_cast<std::size_t>(slot)] =
            server::world::loot::ItemStack{.item = *item, .count = compound->getInt("count", 1)};
    }
    return slots;
}

void write(wire::nbt::NbtCompound& data, const Slots& slots) {
    wire::nbt::NbtList entries{wire::nbt::NbtType::Compound};
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (!slots[slot].has_value()) {
            continue;
        }
        wire::nbt::NbtCompound entry;
        entry.putByte("Slot", static_cast<std::int8_t>(slot));
        entry.putString("id", slots[slot]->item.toString());
        entry.putInt("count", slots[slot]->count);
        entries.push(wire::nbt::NbtTag::ofCompound(std::move(entry)));
    }
    data.set(kItems, wire::nbt::NbtTag::ofList(std::move(entries)));
}

} // namespace basalt::server::runtime::world::blockInventory

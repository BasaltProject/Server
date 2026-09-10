#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <algorithm>
#include <cstdint>
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
        // parity: `ItemStack.CODEC` is `{id, count, components}`, and the components are where an
        // enchantment, a custom name or a written book's pages live. Dropping them here emptied
        // every enchanted book somebody left in a chest.
        const wire::nbt::NbtCompound* components = compound->getCompound("components");
        slots[static_cast<std::size_t>(slot)] = server::world::loot::ItemStack{
            .item = *item,
            .count = compound->getInt("count", 1),
            .components = components == nullptr ? wire::nbt::NbtCompound{} : *components};
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
        // parity: `ItemStack.CODEC` leaves the field out when the item carries nothing of its own.
        if (slots[slot]->components.size() > 0) {
            entry.set("components", wire::nbt::NbtTag::ofCompound(slots[slot]->components));
        }
        entries.push(wire::nbt::NbtTag::ofCompound(std::move(entry)));
    }
    data.set(kItems, wire::nbt::NbtTag::ofList(std::move(entries)));
}

namespace {

/// parity: `DecoratedPotBlockEntity.TAG_ITEM`, the one stack a pot holds, written as an item and not
/// as a list of one.
constexpr std::string_view kSingleItemKey = "item";

} // namespace

bool holdsOneItem(const std::string_view path) { return path == "decorated_pot"; }

Slots readFor(const std::string_view path, const wire::nbt::NbtCompound& data, const std::int32_t size) {
    if (!holdsOneItem(path)) {
        return read(data, size);
    }
    Slots slots(static_cast<std::size_t>(std::max(size, 1)));
    const wire::nbt::NbtTag* tag = data.find(std::string{kSingleItemKey});
    const wire::nbt::NbtCompound* held = tag == nullptr ? nullptr : tag->get<wire::nbt::NbtCompound>();
    if (held == nullptr) {
        return slots;
    }
    const std::string_view id = held->getString("id");
    if (id.empty()) {
        return slots;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
        wire::common::Identifier::parse(std::string{id});
    if (!item.has_value()) {
        return slots;
    }
    const wire::nbt::NbtCompound* components = held->getCompound("components");
    slots[0] =
        server::world::loot::ItemStack{.item = *item,
                                       .count = held->getInt("count", 1),
                                       .components = components == nullptr ? wire::nbt::NbtCompound{} : *components};
    return slots;
}

void writeFor(const std::string_view path, wire::nbt::NbtCompound& data, const Slots& slots) {
    if (!holdsOneItem(path)) {
        write(data, slots);
        return;
    }
    // parity: `saveAdditional` writes nothing at all for an empty pot.
    if (slots.empty() || !slots[0].has_value()) {
        static_cast<void>(data.remove(std::string{kSingleItemKey}));
        return;
    }
    wire::nbt::NbtCompound held;
    held.putString("id", slots[0]->item.toString());
    held.putInt("count", slots[0]->count);
    if (slots[0]->components.size() > 0) {
        held.set("components", wire::nbt::NbtTag::ofCompound(slots[0]->components));
    }
    data.set(std::string{kSingleItemKey}, wire::nbt::NbtTag::ofCompound(std::move(held)));
}

} // namespace basalt::server::runtime::world::blockInventory

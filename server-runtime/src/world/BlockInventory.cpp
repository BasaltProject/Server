#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <string>

namespace basalt::server::runtime::world::blockInventory {
namespace {

constexpr std::string_view kItems = "Items";

} // namespace

std::optional<server::world::loot::ItemStack> readStack(const wire::nbt::NbtCompound& stack) {
    const std::string_view id = stack.getString("id");
    if (id.empty()) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
        wire::common::Identifier::parse(std::string{id});
    if (!item.has_value()) {
        return std::nullopt;
    }
    // parity: `ItemStack.CODEC` is `{id, count, components}`, and the components are where an
    // enchantment, a custom name or a written book's pages live. Dropping them here emptied every
    // enchanted book somebody left in a chest.
    const wire::nbt::NbtCompound* components = stack.getCompound("components");
    return server::world::loot::ItemStack{.item = *item,
                                          .count = stack.getInt("count", 1),
                                          .components = components == nullptr ? wire::nbt::NbtCompound{} : *components};
}

wire::nbt::NbtCompound writeStack(const server::world::loot::ItemStack& stack) {
    wire::nbt::NbtCompound written;
    written.putString("id", stack.item.toString());
    written.putInt("count", stack.count);
    // parity: `ItemStack.CODEC` leaves the field out when the item carries nothing of its own.
    if (stack.components.size() > 0) {
        written.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
    }
    return written;
}

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
        slots[static_cast<std::size_t>(slot)] = readStack(*compound);
    }
    return slots;
}

void write(wire::nbt::NbtCompound& data, const Slots& slots) {
    wire::nbt::NbtList entries{wire::nbt::NbtType::Compound};
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (!slots[slot].has_value()) {
            continue;
        }
        wire::nbt::NbtCompound entry = writeStack(*slots[slot]);
        entry.putByte("Slot", static_cast<std::int8_t>(slot));
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
    slots[0] = readStack(*held);
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
    data.set(std::string{kSingleItemKey}, wire::nbt::NbtTag::ofCompound(writeStack(*slots[0])));
}

} // namespace basalt::server::runtime::world::blockInventory

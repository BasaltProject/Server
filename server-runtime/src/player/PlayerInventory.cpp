#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::nbt::NbtCompound;

constexpr std::int32_t kMenuArmourStart = 5;
constexpr std::int32_t kMenuStorageStart = 9;
constexpr std::int32_t kMenuOffhand = 45;

/// Hotbar first, then the rest of the main inventory: where a picked-up item goes.
[[nodiscard]] const std::array<std::int32_t, PlayerInventory::kMainSlots>& fillOrder() {
    static const std::array<std::int32_t, PlayerInventory::kMainSlots> order = [] {
        std::array<std::int32_t, PlayerInventory::kMainSlots> built{};
        std::size_t at = 0;
        for (std::int32_t slot = PlayerInventory::kHotbarStart; slot < PlayerInventory::kMainSlots; slot++) {
            built[at++] = slot;
        }
        for (std::int32_t slot = PlayerInventory::kHotbarSize; slot < PlayerInventory::kHotbarStart; slot++) {
            built[at++] = slot;
        }
        for (std::int32_t slot = 0; slot < PlayerInventory::kHotbarSize; slot++) {
            built[at++] = slot;
        }
        return built;
    }();
    return order;
}

[[nodiscard]] bool sameStack(const storage::StoredItemStack& left, const storage::StoredItemStack& right) {
    return left.id == right.id && left.components == right.components;
}

const std::optional<storage::StoredItemStack> kNothing;

} // namespace

std::int32_t enchantmentLevel(const storage::StoredItemStack& stack, const Identifier& enchantment) {
    const NbtCompound* stored = stack.components.getCompound(kEnchantmentsComponent);
    if (stored == nullptr) {
        return 0;
    }
    const std::string name = enchantment.toString();
    return stored->contains(name) ? stored->getInt(name) : 0;
}

std::vector<std::pair<Identifier, std::int32_t>> enchantmentLevels(const storage::StoredItemStack& stack) {
    std::vector<std::pair<Identifier, std::int32_t>> levels;
    const NbtCompound* stored = stack.components.getCompound(kEnchantmentsComponent);
    if (stored == nullptr) {
        return levels;
    }
    for (const std::string& name : stored->names()) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(name);
        if (!parsed.has_value()) {
            continue;
        }
        levels.emplace_back(*parsed, stored->getInt(name));
    }
    return levels;
}

PlayerInventory::PlayerInventory(item::itemComponentCodecs::EnchantmentId enchantmentIds)
    : enchantmentIds_(std::move(enchantmentIds)) {}

std::optional<std::int32_t> PlayerInventory::storageSlotOf(std::int32_t menuSlot) {
    if (menuSlot >= kMenuArmourStart && menuSlot < kMenuStorageStart) {
        return menuSlot - kMenuArmourStart + kArmourStart;
    }
    if (menuSlot >= kMenuStorageStart && menuSlot < kMenuOffhand) {
        return menuSlot - kMenuStorageStart;
    }
    return menuSlot == kMenuOffhand ? std::optional{kOffhandSlot} : std::nullopt;
}

std::int32_t PlayerInventory::menuSlotOf(std::int32_t storageSlot) {
    if (storageSlot >= 0 && storageSlot < kArmourStart) {
        return storageSlot + kMenuStorageStart;
    }
    if (storageSlot >= kArmourStart && storageSlot < kOffhandSlot) {
        return storageSlot - kArmourStart + kMenuArmourStart;
    }
    return kMenuOffhand;
}

void PlayerInventory::setHeldSlot(std::int32_t value) { heldSlot_ = std::clamp(value, 0, kHotbarSize - 1); }

const std::optional<storage::StoredItemStack>& PlayerInventory::at(std::int32_t slot) const {
    if (slot < 0 || slot >= static_cast<std::int32_t>(slots_.size())) {
        return kNothing;
    }
    return slots_[static_cast<std::size_t>(slot)];
}

void PlayerInventory::set(std::int32_t slot, std::optional<storage::StoredItemStack> stack) {
    if (slot < 0 || slot >= static_cast<std::int32_t>(slots_.size())) {
        return;
    }
    slots_[static_cast<std::size_t>(slot)] = std::move(stack);
    changed_ = true;
}

const std::optional<storage::StoredItemStack>& PlayerInventory::heldItem() const {
    return slots_[static_cast<std::size_t>(kHotbarStart + heldSlot_)];
}

std::int32_t PlayerInventory::add(const storage::StoredItemStack& stack) {
    std::int32_t remaining = stack.count;
    for (const std::int32_t slot : fillOrder()) {
        if (remaining <= 0) {
            break;
        }
        std::optional<storage::StoredItemStack>& existing = slots_[static_cast<std::size_t>(slot)];
        if (!existing.has_value() || !sameStack(*existing, stack)) {
            continue;
        }
        const std::int32_t room = kMaxStack - existing->count;
        if (room <= 0) {
            continue;
        }
        const std::int32_t moved = std::min(room, remaining);
        existing->count += moved;
        remaining -= moved;
    }
    for (const std::int32_t slot : fillOrder()) {
        if (remaining <= 0) {
            break;
        }
        std::optional<storage::StoredItemStack>& empty = slots_[static_cast<std::size_t>(slot)];
        if (empty.has_value()) {
            continue;
        }
        const std::int32_t moved = std::min(kMaxStack, remaining);
        empty = storage::StoredItemStack{.id = stack.id, .count = moved, .components = stack.components};
        remaining -= moved;
    }
    if (remaining != stack.count) {
        changed_ = true;
    }
    return remaining;
}

std::int32_t PlayerInventory::countOf(std::string_view id) const {
    std::int32_t total = 0;
    for (const std::int32_t slot : fillOrder()) {
        const std::optional<storage::StoredItemStack>& stack = slots_[static_cast<std::size_t>(slot)];
        if (stack.has_value() && stack->id == id) {
            total += stack->count;
        }
    }
    return total;
}

std::int32_t PlayerInventory::take(std::string_view id, std::int32_t count) {
    std::int32_t left = count;
    for (const std::int32_t slot : fillOrder()) {
        if (left <= 0) {
            break;
        }
        std::optional<storage::StoredItemStack>& stack = slots_[static_cast<std::size_t>(slot)];
        if (!stack.has_value() || stack->id != id) {
            continue;
        }
        const std::int32_t taken = std::min(stack->count, left);
        if (stack->count <= taken) {
            stack.reset();
        } else {
            stack->count -= taken;
        }
        left -= taken;
        changed_ = true;
    }
    return count - left;
}

void PlayerInventory::clear() {
    for (std::optional<storage::StoredItemStack>& slot : slots_) {
        slot.reset();
    }
    changed_ = true;
}

std::optional<std::int32_t> PlayerInventory::indexOf(std::string_view id) const {
    for (std::int32_t slot = 0; slot < kMainSlots; slot++) {
        const std::optional<storage::StoredItemStack>& stack = slots_[static_cast<std::size_t>(slot)];
        if (stack.has_value() && stack->id == id) {
            return slot;
        }
    }
    return std::nullopt;
}

std::int32_t PlayerInventory::suitableHotbarSlot() const {
    for (std::int32_t offset = 0; offset < kHotbarSize; offset++) {
        const std::int32_t index = (heldSlot_ + offset) % kHotbarSize;
        if (!slots_[static_cast<std::size_t>(kHotbarStart + index)].has_value()) {
            return index;
        }
    }
    return heldSlot_;
}

std::optional<std::int32_t> PlayerInventory::firstFreeSlot() const {
    for (std::int32_t slot = 0; slot < kMainSlots; slot++) {
        if (!slots_[static_cast<std::size_t>(slot)].has_value()) {
            return slot;
        }
    }
    return std::nullopt;
}

void PlayerInventory::swap(std::int32_t a, std::int32_t b) {
    std::swap(slots_[static_cast<std::size_t>(a)], slots_[static_cast<std::size_t>(b)]);
    changed_ = true;
}

std::vector<std::optional<wire::protocol::play::WireItemStack>> PlayerInventory::toWire() const {
    std::vector<std::optional<wire::protocol::play::WireItemStack>> window;
    window.reserve(kMenuSlots);
    for (std::int32_t menuSlot = 0; menuSlot < kMenuSlots; menuSlot++) {
        const std::optional<std::int32_t> slot = storageSlotOf(menuSlot);
        window.push_back(slot.has_value() ? toWireStack(at(*slot)) : std::nullopt);
    }
    return window;
}

std::optional<wire::protocol::play::WireItemStack> PlayerInventory::toWireSlot(std::int32_t slot) const {
    return toWireStack(at(slot));
}

std::optional<wire::protocol::play::WireItemStack>
PlayerInventory::toWireStack(const std::optional<storage::StoredItemStack>& stack) const {
    if (!stack.has_value()) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(stack->id);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    // An item this build's registry does not know is left out rather than sent as some other item:
    // a wrong id is a wrong item in someone's hand.
    const reg::Item* item = reg::items::registry().byName(*parsed);
    if (item == nullptr) {
        return std::nullopt;
    }
    // An enchantment whose id the client was never sent would draw as a missing entry, so an
    // unresolved one is dropped rather than guessed: the resolver is only set once the enchantment
    // registry has actually gone out.
    const item::itemComponentCodecs::EnchantmentId resolve =
        enchantmentIds_ ? enchantmentIds_ : [](const Identifier&) { return std::optional<std::int32_t>{}; };
    return wire::protocol::play::WireItemStack{.itemId = item->protocolId,
                                               .count = stack->count,
                                               .components =
                                                   item::itemComponentCodecs::toWire(stack->components, resolve)};
}

storage::StoredInventory PlayerInventory::snapshot() const {
    storage::StoredInventory stored;
    stored.slots.assign(slots_.begin(), slots_.end());
    return stored;
}

void PlayerInventory::restore(const storage::StoredInventory& stored) {
    clear();
    for (std::size_t index = 0; index < stored.slots.size() && index < slots_.size(); index++) {
        slots_[index] = stored.slots[index];
    }
}

} // namespace basalt::server::runtime::player

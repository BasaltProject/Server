#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPick.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <optional>

namespace basalt::server::runtime::player {
namespace {

/// parity: `Inventory.findSlotMatchingItem`, which walks vanilla's order: the hotbar, then the rows.
[[nodiscard]] std::optional<std::int32_t> slotMatching(const PlayerInventory& inventory,
                                                       const storage::StoredItemStack& picked) {
    for (std::int32_t index = 0; index < PlayerInventory::kMainSlots; ++index) {
        const std::int32_t slot = index < PlayerInventory::kHotbarSize ? PlayerInventory::kHotbarStart + index
                                                                       : index - PlayerInventory::kHotbarSize;
        const std::optional<storage::StoredItemStack>& stack = inventory.at(slot);
        if (stack.has_value() && stack->count > 0 && stack->id == picked.id && stack->components == picked.components) {
            return slot;
        }
    }
    return std::nullopt;
}

} // namespace

bool pickItemFor(PlayerSession& session, const storage::StoredItemStack& picked) {
    PlayerInventory& inventory = session.inventory;
    const std::optional<std::int32_t> existing = slotMatching(inventory, picked);
    if (existing.has_value() && *existing >= PlayerInventory::kHotbarStart) {
        // Already in the hotbar: vanilla only moves the selection, it does not touch a slot.
        inventory.setHeldSlot(*existing - PlayerInventory::kHotbarStart);
        return true;
    }
    if (existing.has_value()) {
        // parity: `Inventory.pickSlot`, swapped into a suitable hotbar slot, not duplicated.
        inventory.setHeldSlot(inventory.suitableHotbarSlot());
        inventory.swap(PlayerInventory::kHotbarStart + inventory.heldSlot(), *existing);
        return true;
    }
    if (session.gamemode != wire::protocol::play::Gamemode::Creative) {
        return false;
    }
    // parity: `Inventory.addAndPickItem`: what is in the way moves to a free slot, or is overwritten.
    inventory.setHeldSlot(inventory.suitableHotbarSlot());
    const std::int32_t target = PlayerInventory::kHotbarStart + inventory.heldSlot();
    const std::optional<storage::StoredItemStack> displaced = inventory.at(target);
    if (displaced.has_value()) {
        const std::optional<std::int32_t> free = inventory.firstFreeSlot();
        if (free.has_value()) {
            inventory.set(*free, displaced);
        }
    }
    inventory.set(target, picked);
    return true;
}

bool pickItemFor(PlayerSession& session, const std::string& id) {
    return pickItemFor(session, storage::StoredItemStack{.id = id, .count = 1, .components = {}});
}

} // namespace basalt::server::runtime::player

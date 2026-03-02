#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPick.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <optional>

namespace basalt::server::runtime::player {

bool pickItemFor(PlayerSession& session, const std::string& id) {
    PlayerInventory& inventory = session.inventory;
    const std::optional<std::int32_t> existing = inventory.indexOf(id);
    if (existing.has_value() && *existing >= PlayerInventory::kHotbarStart) {
        // Already in the hotbar: vanilla only moves the selection, it does not touch a slot.
        inventory.setHeldSlot(*existing - PlayerInventory::kHotbarStart);
        return true;
    }
    if (existing.has_value()) {
        // Elsewhere in the inventory: swapped into a suitable hotbar slot, not duplicated.
        inventory.setHeldSlot(inventory.suitableHotbarSlot());
        inventory.swap(PlayerInventory::kHotbarStart + inventory.heldSlot(), *existing);
        return true;
    }
    if (session.gamemode != wire::protocol::play::Gamemode::Creative) {
        return false;
    }
    // Not carried at all: only creative may conjure one, and only ever a single item.
    inventory.setHeldSlot(inventory.suitableHotbarSlot());
    const std::int32_t target = PlayerInventory::kHotbarStart + inventory.heldSlot();
    // Every hotbar slot was full, so whatever is in the way moves to a free slot rather than being
    // destroyed; if nothing is free, vanilla overwrites it too.
    const std::optional<storage::StoredItemStack> displaced = inventory.at(target);
    if (displaced.has_value()) {
        const std::optional<std::int32_t> free = inventory.firstFreeSlot();
        if (free.has_value()) {
            inventory.set(*free, displaced);
        }
    }
    inventory.set(target, storage::StoredItemStack{.id = id, .count = 1, .components = {}});
    return true;
}

} // namespace basalt::server::runtime::player

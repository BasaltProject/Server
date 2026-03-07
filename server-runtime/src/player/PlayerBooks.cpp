#include <basalt/server/runtime/player/PlayerBooks.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Books.hpp>

namespace basalt::server::runtime::player {
namespace {

/// parity: `Inventory.isHotbarSlot(slot) || slot == 40`, and nothing else is written to.
[[nodiscard]] std::optional<std::int32_t> storageSlotOf(const std::int32_t slot) {
    if (slot >= 0 && slot < PlayerInventory::kHotbarSize) {
        return PlayerInventory::kHotbarStart + slot;
    }
    return slot == PlayerInventory::kOffhandSlot ? std::optional{PlayerInventory::kOffhandSlot} : std::nullopt;
}

} // namespace

bool editBookPageFor(PlayerSession& session, const std::int32_t slot, const world::books::PageEdit what,
                     const std::int32_t page, const std::int32_t other, const std::string& text) {
    const std::optional<std::int32_t> where = storageSlotOf(slot);
    if (!where.has_value() || page < 0 || other < 0) {
        return false;
    }
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(*where);
    if (!held.has_value() || !world::books::isWritable(*held)) {
        return false;
    }
    std::vector<std::string> pages = world::books::pagesOf(*held);
    if (!world::books::editPage(pages, what, static_cast<std::size_t>(page), static_cast<std::size_t>(other), text)) {
        return false;
    }
    storage::StoredItemStack written = *held;
    if (!world::books::writePages(written, pages)) {
        return false;
    }
    session.inventory.set(*where, std::move(written));
    return true;
}

bool editBookFor(PlayerSession& session, const std::int32_t slot, const std::vector<std::string>& pages,
                 const std::optional<std::string>& title) {
    const std::optional<std::int32_t> where = storageSlotOf(slot);
    if (!where.has_value()) {
        return false;
    }
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(*where);
    if (!held.has_value() || !world::books::isWritable(*held)) {
        return false;
    }
    if (!title.has_value()) {
        storage::StoredItemStack written = *held;
        if (!world::books::writePages(written, pages)) {
            return false;
        }
        session.inventory.set(*where, std::move(written));
        return true;
    }
    // parity: `signBook` signs what the same packet just wrote, so the pages go on first.
    storage::StoredItemStack written = *held;
    if (!world::books::writePages(written, pages)) {
        return false;
    }
    std::optional<storage::StoredItemStack> signed_ = world::books::sign(written, *title, session.name());
    if (!signed_.has_value()) {
        return false;
    }
    session.inventory.set(*where, std::move(*signed_));
    return true;
}

} // namespace basalt::server::runtime::player

#pragma once

#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {

/// Where an enchanted stack keeps its levels, in the same component name the wire uses.
inline constexpr std::string_view kEnchantmentsComponent = "minecraft:enchantments";

/// The level `enchantment` is on `stack`, or zero when it is not on it at all.
[[nodiscard]] std::int32_t enchantmentLevel(const storage::StoredItemStack& stack,
                                            const wire::common::Identifier& enchantment);

/// Every enchantment on `stack` with its level, by name.
[[nodiscard]] std::vector<std::pair<wire::common::Identifier, std::int32_t>>
enchantmentLevels(const storage::StoredItemStack& stack);

/// The 41 slots of one player: 36 main including the hotbar, 4 armour, 1 offhand, stored in the
/// vanilla network order so persistence is a straight copy. Only the container: what a player can
/// do with the stacks lives elsewhere.
class PlayerInventory {
public:
    /// How many slots container 0 has on the wire: `InventoryMenu`'s 46 (result, 2x2 grid, armour,
    /// storage, hotbar, offhand), not the 41 stored here. Sending 41 makes the client drop the window
    /// packet, and its clicks are numbered in menu slots.
    static constexpr std::int32_t kMenuSlots = 46;

    /// Where the four worn pieces sit, head first, in this class's own numbering.
    static constexpr std::int32_t kArmourStart = 36;
    static constexpr std::int32_t kArmourSlots = 4;

    /// Where the off-hand stack lives in this class's own numbering.
    static constexpr std::int32_t kOffhandSlot = 40;

    /// Storage rows plus hotbar, the part `Inventory.findSlotMatchingItem`/`getFreeSlot` scan.
    /// Armour and offhand come after.
    static constexpr std::int32_t kMainSlots = 36;

    static constexpr std::int32_t kHotbarSize = 9;

    /// Where the hotbar starts in the network order: after the 27 storage rows.
    static constexpr std::int32_t kHotbarStart = kMainSlots - kHotbarSize;

    /// What any one slot holds at most, before the stack's own limit. parity: `Container.getMaxStackSize`.
    static constexpr std::int32_t kSlotLimit = 99;

    /// How many of `stack` one slot takes. parity: `Container.getMaxStackSize(ItemStack)`.
    [[nodiscard]] static std::int32_t maxStackIn(const storage::StoredItemStack& stack);

    /// Enchantment name to registry index, or unset while the enchantment registry has not been
    /// sent. Handed in rather than looked up: this class knows nothing about the server around it.
    explicit PlayerInventory(item::itemComponentCodecs::EnchantmentId enchantmentIds = {});

    /// This class's own slot index for a container-0 slot, or nothing for the five slots it does not
    /// model (the crafting result and the 2x2 grid, which this build has no crafting for).
    [[nodiscard]] static std::optional<std::int32_t> storageSlotOf(std::int32_t menuSlot);

    /// The container-0 slot a storage slot appears in. The inverse of `storageSlotOf`.
    [[nodiscard]] static std::int32_t menuSlotOf(std::int32_t storageSlot);

    /// Set by every write, cleared by whoever acted on it.
    ///
    /// parity: `Inventory.setChanged`, which is what vanilla's inventory trigger listens to: an
    /// advancement is not checked per item moved but once per tick, after everything has moved.
    [[nodiscard]] bool changed() const noexcept { return changed_; }

    void clearChanged() noexcept { changed_ = false; }

    /// Which hotbar slot is in hand, 0 to 8.
    [[nodiscard]] std::int32_t heldSlot() const noexcept { return heldSlot_; }

    void setHeldSlot(std::int32_t value);

    [[nodiscard]] const std::optional<storage::StoredItemStack>& at(std::int32_t slot) const;

    void set(std::int32_t slot, std::optional<storage::StoredItemStack> stack);

    [[nodiscard]] const std::optional<storage::StoredItemStack>& heldItem() const;

    /// Adds a stack the way picking something up does: into a matching stack first, then the first
    /// empty slot, each up to the stack's own limit. Returns how many items would not fit.
    [[nodiscard]] std::int32_t add(const storage::StoredItemStack& stack);

    /// How many of an item this player is carrying, over every slot the inventory fills from.
    [[nodiscard]] std::int32_t countOf(std::string_view id) const;

    /// Takes `count` of an item out, from the first slot that has any. Answers how many were
    /// actually taken, which is what the caller checks before it hands anything over in exchange.
    [[nodiscard]] std::int32_t take(std::string_view id, std::int32_t count);

    void clear();

    /// The first slot carrying an item, or nothing if this player has none: what "pick block"
    /// searches for. parity: `Inventory.findSlotMatchingItem`, which scans only the main inventory,
    /// so armour and the offhand deliberately do not answer here either.
    [[nodiscard]] std::optional<std::int32_t> indexOf(std::string_view id) const;

    /// The slot a bow draws its arrow from. parity: `Player.getProjectile`: the off hand, then the
    /// main hand, then the bag from the hotbar on, taking any of `#minecraft:arrows`.
    [[nodiscard]] std::optional<std::int32_t> arrowSlot() const;

    /// The hotbar slot a picked item should go to, as a 0-8 index. parity:
    /// `Inventory.getSuitableHotbarSlot`: the first empty slot scanning from the held one and
    /// wrapping. The full-hotbar fallback (prefer an unenchanted slot) collapses to the held slot
    /// here because enchantments are not weighed.
    [[nodiscard]] std::int32_t suitableHotbarSlot() const;

    /// The first empty main-inventory slot. parity: `Inventory.getFreeSlot`.
    [[nodiscard]] std::optional<std::int32_t> firstFreeSlot() const;

    /// Swaps the stacks at two slot indices, both in range.
    void swap(std::int32_t a, std::int32_t b);

    /// Every slot as container 0 wants them: `kMenuSlots` entries in `InventoryMenu`'s own order,
    /// not this class's storage order. See `menuSlotOf` for why those differ.
    [[nodiscard]] std::vector<std::optional<wire::protocol::play::WireItemStack>> toWire() const;

    [[nodiscard]] std::optional<wire::protocol::play::WireItemStack> toWireSlot(std::int32_t slot) const;

    /// One arbitrary stack in wire form, for things held outside the slots (the cursor during a
    /// click).
    [[nodiscard]] std::optional<wire::protocol::play::WireItemStack>
    toWireStack(const std::optional<storage::StoredItemStack>& stack) const;

    [[nodiscard]] storage::StoredInventory snapshot() const;

    void restore(const storage::StoredInventory& stored);

private:
    [[nodiscard]] std::optional<std::int32_t> slotWithRoomFor(const storage::StoredItemStack& stack) const;

    item::itemComponentCodecs::EnchantmentId enchantmentIds_;
    std::array<std::optional<storage::StoredItemStack>, storage::StoredInventory::kPlayerSlots> slots_;
    bool changed_ = true;
    std::int32_t heldSlot_ = 0;
};

} // namespace basalt::server::runtime::player

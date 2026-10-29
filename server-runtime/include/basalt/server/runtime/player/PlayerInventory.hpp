#pragma once

#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {

inline constexpr std::string_view kEnchantmentsComponent = "minecraft:enchantments";

[[nodiscard]] std::int32_t enchantmentLevel(const storage::StoredItemStack& stack,
                                            const wire::common::Identifier& enchantment);

[[nodiscard]] std::vector<std::pair<wire::common::Identifier, std::int32_t>>
enchantmentLevels(const storage::StoredItemStack& stack);

class PlayerInventory {
public:
    static constexpr std::int32_t kMenuSlots = 46;

    static constexpr std::int32_t kArmourStart = 36;
    static constexpr std::int32_t kArmourSlots = 4;

    static constexpr std::int32_t kOffhandSlot = 40;

    static constexpr std::int32_t kMainSlots = 36;

    static constexpr std::int32_t kHotbarSize = 9;

    /// Where the hotbar starts in the network order: after the 27 storage rows.
    static constexpr std::int32_t kHotbarStart = kMainSlots - kHotbarSize;

    /// What any one slot holds at most, before the stack's own limit. parity: `Container.getMaxStackSize`.
    static constexpr std::int32_t kSlotLimit = 99;

    [[nodiscard]] static std::int32_t maxStackIn(const storage::StoredItemStack& stack,
                                                 std::int32_t slotLimit = kSlotLimit);

    /// A plugin's own cap on what one slot of this inventory holds; never above the 99 of `kSlotLimit`.
    [[nodiscard]] std::int32_t slotLimit() const noexcept { return slotLimit_; }

    void setSlotLimit(std::int32_t value) noexcept { slotLimit_ = std::clamp(value, 1, kSlotLimit); }

    explicit PlayerInventory(item::itemComponentCodecs::EnchantmentId enchantmentIds = {});

    [[nodiscard]] static std::optional<std::int32_t> storageSlotOf(std::int32_t menuSlot);

    [[nodiscard]] static std::int32_t menuSlotOf(std::int32_t storageSlot);

    /// parity: `Inventory.setChanged`.
    [[nodiscard]] bool changed() const noexcept { return changed_; }

    void clearChanged() noexcept { changed_ = false; }

    [[nodiscard]] std::int32_t heldSlot() const noexcept { return heldSlot_; }

    void setHeldSlot(std::int32_t value);

    [[nodiscard]] const std::optional<storage::StoredItemStack>& at(std::int32_t slot) const;

    void set(std::int32_t slot, std::optional<storage::StoredItemStack> stack);

    [[nodiscard]] const std::optional<storage::StoredItemStack>& heldItem() const;

    [[nodiscard]] std::int32_t add(const storage::StoredItemStack& stack);

    /// Whether `add` would take a single item of this stack, without touching anything.
    [[nodiscard]] bool hasRoomForOne(const storage::StoredItemStack& stack) const;

    [[nodiscard]] std::int32_t countOf(std::string_view id) const;

    [[nodiscard]] std::int32_t take(std::string_view id, std::int32_t count);

    void clear();

    /// searches for. parity: `Inventory.findSlotMatchingItem`, which scans only the main inventory.
    [[nodiscard]] std::optional<std::int32_t> indexOf(std::string_view id) const;

    [[nodiscard]] std::optional<std::int32_t> arrowSlot() const;

    [[nodiscard]] std::int32_t suitableHotbarSlot() const;

    [[nodiscard]] std::optional<std::int32_t> firstFreeSlot() const;

    void swap(std::int32_t a, std::int32_t b);

    [[nodiscard]] std::vector<std::optional<wire::protocol::play::WireItemStack>> toWire() const;

    [[nodiscard]] std::optional<wire::protocol::play::WireItemStack> toWireSlot(std::int32_t slot) const;

    [[nodiscard]] std::optional<wire::protocol::play::WireItemStack>
    toWireStack(const std::optional<storage::StoredItemStack>& stack) const;

    [[nodiscard]] storage::StoredInventory snapshot() const;

    void restore(const storage::StoredInventory& stored);

    /// parity: `Inventory.getSlotWithRemainingSpace`.
    [[nodiscard]] std::optional<std::int32_t> slotWithRoomFor(const storage::StoredItemStack& stack) const;

private:
    item::itemComponentCodecs::EnchantmentId enchantmentIds_;
    std::array<std::optional<storage::StoredItemStack>, storage::StoredInventory::kPlayerSlots> slots_;
    bool changed_ = true;
    std::int32_t heldSlot_ = 0;
    std::int32_t slotLimit_ = kSlotLimit;
};

} // namespace basalt::server::runtime::player

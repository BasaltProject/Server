#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::server::runtime::player {

namespace menuSlot {

struct Inventory {
    std::int32_t index = 0;

    friend bool operator==(const Inventory&, const Inventory&) = default;
};

struct Grid {
    std::int32_t index = 0;

    friend bool operator==(const Grid&, const Grid&) = default;
};

/// The crafting result. Can be taken from, never placed into.
struct Result {
    friend bool operator==(const Result&, const Result&) = default;
};

struct Block {
    std::int32_t index = 0;

    friend bool operator==(const Block&, const Block&) = default;
};

} // namespace menuSlot

using MenuSlot = std::variant<menuSlot::Inventory, menuSlot::Grid, menuSlot::Result, menuSlot::Block>;

class Menu {
public:
    static constexpr std::int32_t kColumns = 9;

    static constexpr std::int32_t kChestSize = 27;

    static constexpr std::int32_t kDoubleChestSize = 54;

    static constexpr std::int32_t kCrafterSize = 9;

    static constexpr std::int32_t kStonecutterInput = 0;
    static constexpr std::int32_t kStonecutterResult = 1;

    static constexpr std::int32_t kAnvilInput = 0;
    static constexpr std::int32_t kAnvilAddition = 1;
    static constexpr std::int32_t kAnvilResult = 2;

    static constexpr std::int32_t kLoomBanner = 0;
    static constexpr std::int32_t kLoomDye = 1;
    static constexpr std::int32_t kLoomPattern = 2;
    static constexpr std::int32_t kLoomResult = 3;

    static constexpr std::int32_t kBeaconPayment = 0;

    static constexpr std::int32_t kEnchantmentInput = 0;
    static constexpr std::int32_t kEnchantmentLapis = 1;

    static constexpr std::int32_t kCartographyMap = 0;
    static constexpr std::int32_t kCartographyAdditional = 1;
    static constexpr std::int32_t kCartographyResult = 2;

    static constexpr std::int32_t kGrindstoneInput = 0;
    static constexpr std::int32_t kGrindstoneAddition = 1;
    static constexpr std::int32_t kGrindstoneResult = 2;

    static constexpr std::int32_t kSmithingTemplate = 0;
    static constexpr std::int32_t kSmithingBase = 1;
    static constexpr std::int32_t kSmithingAddition = 2;
    static constexpr std::int32_t kSmithingResult = 3;

    Menu(std::int32_t containerId, std::int32_t gridWidth, std::int32_t gridHeight,
         std::vector<std::optional<MenuSlot>> slots);

    [[nodiscard]] std::int32_t containerId() const noexcept { return containerId_; }

    [[nodiscard]] std::int32_t gridWidth() const noexcept { return gridWidth_; }

    [[nodiscard]] std::int32_t gridHeight() const noexcept { return gridHeight_; }

    [[nodiscard]] std::int32_t slotCount() const noexcept { return static_cast<std::int32_t>(slots_.size()); }

    [[nodiscard]] std::int32_t gridSize() const noexcept { return gridWidth_ * gridHeight_; }

    [[nodiscard]] std::optional<MenuSlot> slotAt(std::int32_t menuSlot) const;

    [[nodiscard]] std::optional<std::int32_t> menuSlotOf(const MenuSlot& slot) const;

    [[nodiscard]] static Menu playerInventory();

    [[nodiscard]] static Menu chest(std::int32_t containerId);

    [[nodiscard]] static Menu doubleChest(std::int32_t containerId);

    [[nodiscard]] static Menu rows(std::int32_t containerId, std::int32_t rows);

    [[nodiscard]] static Menu mount(std::int32_t containerId, std::int32_t columns);

    [[nodiscard]] static Menu stonecutter(std::int32_t containerId);

    [[nodiscard]] static Menu smithing(std::int32_t containerId);

    [[nodiscard]] static Menu anvil(std::int32_t containerId);

    [[nodiscard]] static Menu grindstone(std::int32_t containerId);

    [[nodiscard]] static Menu enchantment(std::int32_t containerId);

    [[nodiscard]] static Menu beacon(std::int32_t containerId);

    [[nodiscard]] static Menu loom(std::int32_t containerId);

    [[nodiscard]] static Menu cartographyTable(std::int32_t containerId);

    [[nodiscard]] static Menu merchant(std::int32_t containerId);

    [[nodiscard]] static Menu crafter(std::int32_t containerId);

    [[nodiscard]] static Menu furnace(std::int32_t containerId);

    [[nodiscard]] static Menu brewingStand(std::int32_t containerId);

    [[nodiscard]] static Menu craftingTable(std::int32_t containerId);

private:
    std::int32_t containerId_ = 0;
    std::int32_t gridWidth_ = 0;
    std::int32_t gridHeight_ = 0;
    std::vector<std::optional<MenuSlot>> slots_;
};

} // namespace basalt::server::runtime::player

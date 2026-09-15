#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::server::runtime::player {

/// What a slot of an open window actually refers to.
namespace menuSlot {

/// A slot of the player's own inventory, in `PlayerInventory`'s storage numbering.
struct Inventory {
    std::int32_t index = 0;

    friend bool operator==(const Inventory&, const Inventory&) = default;
};

/// A cell of the crafting grid, row-major.
struct Grid {
    std::int32_t index = 0;

    friend bool operator==(const Grid&, const Grid&) = default;
};

/// The crafting result. Can be taken from, never placed into.
struct Result {
    friend bool operator==(const Result&, const Result&) = default;
};

/// A slot of the block the window belongs to: a furnace's input, fuel and result.
///
/// The block keeps its own contents, so the session only says which of them a click meant.
struct Block {
    std::int32_t index = 0;

    friend bool operator==(const Block&, const Block&) = default;
};

} // namespace menuSlot

/// A variant rather than a hierarchy: there is no RTTI here to ask what a pointer really is, and
/// four cases that never grow do not need one.
using MenuSlot = std::variant<menuSlot::Inventory, menuSlot::Grid, menuSlot::Result, menuSlot::Block>;

/// The slot layout of one open window.
///
/// The client addresses everything by its position in *this* layout, which is neither the server's
/// storage order nor the same between windows: the player's own inventory puts a 2x2 grid and the
/// armour where the crafting table puts a 3x3 grid, and both are 46 slots long. Getting this
/// mapping wrong does not fail loudly, it just moves the wrong item, so it lives in one place with
/// both layouts written out rather than being computed at each call site.
///
/// parity: `InventoryMenu` and `CraftingMenu`, whose own slot order this mirrors.
class Menu {
public:
    /// Nine to a row, which is every window the client draws.
    static constexpr std::int32_t kColumns = 9;

    /// parity: `ChestMenu`'s own three rows of nine.
    static constexpr std::int32_t kChestSize = 27;

    /// parity: `ChestMenu.sixRows`, the double chest.
    static constexpr std::int32_t kDoubleChestSize = 54;

    /// parity: `CrafterMenu`'s own nine.
    static constexpr std::int32_t kCrafterSize = 9;

    /// The stonecutter's two slots: what goes in, and what the chosen button makes of it.
    static constexpr std::int32_t kStonecutterInput = 0;
    static constexpr std::int32_t kStonecutterResult = 1;

    /// parity: `AnvilMenu`'s own three: what is being worked on, what is going onto it, and what
    /// comes out.
    static constexpr std::int32_t kAnvilInput = 0;
    static constexpr std::int32_t kAnvilAddition = 1;
    static constexpr std::int32_t kAnvilResult = 2;

    /// parity: `LoomMenu`'s four: the banner, the dye, the pattern and what it weaves.
    static constexpr std::int32_t kLoomBanner = 0;
    static constexpr std::int32_t kLoomDye = 1;
    static constexpr std::int32_t kLoomPattern = 2;
    static constexpr std::int32_t kLoomResult = 3;

    /// parity: `BeaconMenu.PAYMENT_SLOT`, the one slot a beacon has.
    static constexpr std::int32_t kBeaconPayment = 0;

    /// parity: `EnchantmentMenu`'s two: what is being enchanted, and what is paying for it.
    static constexpr std::int32_t kEnchantmentInput = 0;
    static constexpr std::int32_t kEnchantmentLapis = 1;

    /// parity: `GrindstoneMenu`'s own three, which are numbered like an anvil's.
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

    /// What this window's slot refers to, or nothing for one this build does not model.
    [[nodiscard]] std::optional<MenuSlot> slotAt(std::int32_t menuSlot) const;

    /// Where a slot appears in this window, or nothing if this window does not show it.
    [[nodiscard]] std::optional<std::int32_t> menuSlotOf(const MenuSlot& slot) const;

    /// The player's own window, always open, always container 0: result, 2x2 grid, armour, storage
    /// rows, hotbar, offhand.
    [[nodiscard]] static Menu playerInventory();

    /// A chest, a trapped chest or a barrel: its own rows first, then the player's storage and
    /// hotbar. The block keeps the contents, so a click only says which of its slots was meant.
    [[nodiscard]] static Menu chest(std::int32_t containerId);

    /// Two chests as one window. The first 27 slots are the *first* half as vanilla orders them,
    /// which is not always the one that was clicked: see `ChestBlock.DOUBLE_INVENTORY_COMBINER`.
    [[nodiscard]] static Menu doubleChest(std::int32_t containerId);

    /// A window of `rows` rows of nine, then the player's own 36: `minecraft:generic_9xN`.
    ///
    /// What a plugin's menu is drawn in. The slots above the player's are `menuSlot::Block` like a
    /// chest's, because the *shape* is a chest's; what is behind them is not a block, which is the
    /// caller's business and not this class's.
    [[nodiscard]] static Menu rows(std::int32_t containerId, std::int32_t rows);

    /// parity: `StonecutterMenu`: one slot in, one out, then the player's 36.
    [[nodiscard]] static Menu stonecutter(std::int32_t containerId);

    /// parity: `SmithingMenu`: template, base, addition, result, then the player's 36.
    [[nodiscard]] static Menu smithing(std::int32_t containerId);

    /// parity: `AnvilMenu`: input, addition, result, then the player's 36.
    [[nodiscard]] static Menu anvil(std::int32_t containerId);

    /// parity: `GrindstoneMenu`: the same three slots, and no price to pay for them.
    [[nodiscard]] static Menu grindstone(std::int32_t containerId);

    /// parity: `EnchantmentMenu`: what is being enchanted and the lapis, then the player's 36.
    [[nodiscard]] static Menu enchantment(std::int32_t containerId);

    /// parity: `BeaconMenu`: the payment slot, then the player's 36.
    [[nodiscard]] static Menu beacon(std::int32_t containerId);

    /// parity: `LoomMenu`: banner, dye, pattern, result, then the player's 36.
    [[nodiscard]] static Menu loom(std::int32_t containerId);

    /// A villager's window. The three trading slots are the menu's own rather than any block's, so
    /// nothing behind them is a container: the trade itself is what moves the items.
    [[nodiscard]] static Menu merchant(std::int32_t containerId);

    /// A crafter: its nine slots, then the player's storage and hotbar. There is no result slot,
    /// because nobody takes anything out of a crafter by hand: it pushes what it makes.
    [[nodiscard]] static Menu crafter(std::int32_t containerId);

    /// A furnace, a blast furnace or a smoker: input, fuel, result, then the player's storage and
    /// hotbar. parity: `AbstractFurnaceMenu`'s own slot order.
    [[nodiscard]] static Menu furnace(std::int32_t containerId);

    /// A crafting table: result, 3x3 grid, then the player's storage and hotbar. No armour and no
    /// offhand, which is exactly why its numbering cannot be shared with the player's own window.
    [[nodiscard]] static Menu craftingTable(std::int32_t containerId);

private:
    std::int32_t containerId_ = 0;
    std::int32_t gridWidth_ = 0;
    std::int32_t gridHeight_ = 0;
    std::vector<std::optional<MenuSlot>> slots_;
};

} // namespace basalt::server::runtime::player

#pragma once

#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

/// One item a plugin drew into a window it opened.
struct PluginMenuItem {
    std::string id;
    std::int32_t count = 1;
    /// MiniMessage on the plugin's side, parsed into a component when the window is drawn.
    std::optional<std::string> name;
    /// One line each, the same way.
    std::vector<std::string> lore;
};

/// Container, menu, inventory and crafting state: every window this player can have open, and what
/// a click, a shift-click or a drag does to it.
///
/// Kept as one class: reading a slot, writing one, and deciding what may go into it all switch on
/// the same mutually exclusive "which window is open" fields, and splitting that check across files
/// would either duplicate it or reintroduce a shared state holder anyway.
class ContainerMenuController {
public:
    /// parity: the four grid cells of a player's own inventory, and the nine of a table.
    static constexpr std::int32_t kMaxGridSize = 9;

    /// What a plugin gets a say in. Unset means nobody objects.
    struct Hooks {
        std::function<bool(std::string_view kind, const std::optional<wire::protocol::play::BlockPos>& pos)>
            openAllowed;
        /// The slot, the button and the mode, plus what is in that slot when it is a player's own.
        std::function<bool(std::int32_t slot, std::int32_t button, std::string_view mode,
                           const std::optional<std::string>& item)>
            clickAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item)>
            pickAllowed;
    };

    /// The rolls opening and dropping make.
    struct Options {
        std::function<double()> nextDouble;
        std::function<float()> nextFloat;
        std::function<std::int64_t()> nextSeed;
    };

    explicit ContainerMenuController(ClientSession& owner, Hooks hooks = {}, Options options = {});

    /// The window this player has open. Their own inventory unless they opened a block's.
    player::Menu openMenu = player::Menu::playerInventory();

    /// The last container id handed out, so a freshly opened window never reuses the previous one.
    std::int32_t nextContainerId = 1;

    /// The block whose window is open, and the other half of a double chest.
    std::optional<wire::protocol::play::BlockPos> openBlock;
    std::optional<wire::protocol::play::BlockPos> openBlockSecond;

    /// How many slots the open block has, which is what a click is bounds-checked against.
    std::int32_t openBlockSize = world::furnaces::kSize;

    /// The lectern whose page this player is turning, if that is what is open.
    std::optional<wire::protocol::play::BlockPos> openLecternAt;

    void openCraftingTable(player::PlayerSession& session);

    void openFurnace(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                     const std::string& cooker);

    void openEnderChest(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openChest(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, const std::string& block);

    void openSmithingMenu(player::PlayerSession& session);

    void openAnvilMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openGrindstoneMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openEnchantmentMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// parity: `ServerboundRenameItemPacket`, which arrives on every keystroke and works the result
    /// out again each time.
    void setAnvilName(const std::string& name);

    void openStonecutterMenu(player::PlayerSession& session);

    void openCrafter(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// Opens a window a plugin drew. The plugin bridge's entry point, so the shape stays exact.
    void openPluginMenu(const std::string& title, std::int32_t rows,
                        const std::map<std::int32_t, PluginMenuItem>& items, std::function<void(std::int32_t)> onClick);

    void refreshPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items);

    void closeOpenMenu();

    void closeOpenMenu(player::PlayerSession& session);

    /// parity: `ContainerButtonClick`, which is how a lectern's page and a stonecutter's choice
    /// arrive.
    void clickContainerButton(std::int32_t button);

    /// parity: `handleContainerSlotStateChanged`, which only the crafter listens to: it switches one
    /// of its nine slots on or off, and a comparator counts a slot that is off as a full one.
    void setSlotState(std::int32_t slot, bool enabled);

    /// Re-sends whatever the open block holds, for a change made on the server side.
    void syncOpenContainer();

    /// The two progress bars a furnace window draws.
    void sendFurnaceBars();

    /// Empties the crafting grid back into the player.
    void returnCraftingGrid(player::PlayerSession& session);

    void handleContainerClick(const wire::protocol::play::ServerboundContainerClickPacket& packet);

    void handleSetCreativeModeSlot(const wire::protocol::play::ServerboundSetCreativeModeSlotPacket& packet);

    void handlePickItemFromBlock(const wire::protocol::play::PickItemFromBlockPacket& packet);

    void handlePickItemFromEntity(const wire::protocol::play::PickItemFromEntityPacket& packet);

    void sendInventory();

    void sendInventoryAll();

    void sendInventorySlot(std::int32_t slot);

private:
    [[nodiscard]] std::int32_t allocateContainerId();

    [[nodiscard]] std::optional<storage::StoredItemStack> readBlockSlot(std::int32_t index) const;

    void writeBlockSlot(std::int32_t index, const std::optional<storage::StoredItemStack>& stack);

    [[nodiscard]] std::optional<storage::StoredItemStack> readSlot(const player::PlayerSession& session,
                                                                   const player::MenuSlot& slot) const;

    void writeSlot(player::PlayerSession& session, const player::MenuSlot& slot,
                   const std::optional<storage::StoredItemStack>& stack);

    [[nodiscard]] bool canPlaceInto(const player::MenuSlot& slot) const;

    [[nodiscard]] bool hasBlockSlots() const;

    [[nodiscard]] bool accepts(const player::MenuSlot& target, const storage::StoredItemStack& stack) const;

    void recomputeCraftingResult();

    [[nodiscard]] std::vector<std::optional<storage::StoredItemStack>> craftingRemainders() const;

    void takeCraftingResult(player::PlayerSession& session);

    void recomputeSmithingResult();

    void takeSmithingResult();

    /// parity: `EnchantmentMenu.slotsChanged`, which rolls all three rows off the player's seed.
    void rollEnchantmentOffers();

    /// parity: `EnchantmentMenu.clickMenuButton`, the row they pressed.
    void takeEnchantmentOffer(player::PlayerSession& session, std::int32_t row);

    void sendEnchantmentNumbers();

    void recomputeGrindstoneResult();

    void takeGrindstoneResult(player::PlayerSession& session);

    void recomputeAnvilResult();

    void takeAnvilResult(player::PlayerSession& session);

    /// parity: `AnvilBlock.damage`, the eight chances in a hundred that an anvil survives a use.
    void wearAnvil();

    void sendAnvilCost();

    /// parity: `Slot.mayPickup`: an anvil's result is only picked up by somebody who can pay for it.
    [[nodiscard]] bool mayPickup(const player::MenuSlot& slot) const;

    void selectStonecutterRecipe(std::int32_t button);

    void takeStonecutterResult();

    void pickupClick(player::PlayerSession& session, const player::MenuSlot& slot, bool half);

    void quickMove(player::PlayerSession& session, const player::MenuSlot& slot);

    void collectMatching(player::PlayerSession& session, bool reversed);

    bool handleQuickCraft(player::PlayerSession& session,
                          const wire::protocol::play::ServerboundContainerClickPacket& packet);

    void distributeQuickCraft(player::PlayerSession& session);

    void dropStack(player::PlayerSession& session, const storage::StoredItemStack& stack);

    void pickItem(player::PlayerSession& session, const std::string& id);

    void announceOpeners(const wire::protocol::play::BlockPos& pos, const std::string& block, std::int32_t openers);

    void fillPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items);

    [[nodiscard]] std::optional<world::furnaces::State> furnaceState() const;

    /// Which half of a double chest a menu slot belongs to, and where in it.
    [[nodiscard]] std::optional<std::pair<wire::protocol::play::BlockPos, std::int32_t>>
    halfFor(std::int32_t index) const;

    [[nodiscard]] std::int32_t halfSize() const;

    ClientSession* owner_;
    Hooks hooks_;
    Options options_;

    /// Vanilla's optimistic-locking counter for container changes: bumped on every window write so
    /// the client can tell a click made against the current contents from a stale one.
    std::int32_t containerStateId_ = 0;

    std::array<std::optional<storage::StoredItemStack>, kMaxGridSize> craftingGrid_;
    std::optional<storage::StoredItemStack> craftingResult_;
    std::optional<storage::StoredItemStack> carriedItem_;

    bool openEnderChest_ = false;
    std::optional<std::vector<std::optional<storage::StoredItemStack>>> pluginMenu_;
    std::function<void(std::int32_t)> pluginMenuClick_;
    std::optional<wire::protocol::play::BlockPos> openedAt_;

    bool openStonecutter_ = false;
    std::optional<storage::StoredItemStack> stonecutterInput_;
    std::optional<storage::StoredItemStack> stonecutterResult_;

    bool openSmithing_ = false;
    std::array<std::optional<storage::StoredItemStack>, 4> smithingSlots_;

    bool openEnchantment_ = false;
    std::array<std::optional<storage::StoredItemStack>, 2> enchantmentSlots_;
    /// What each of the three rows costs, and what it is hinting at: the enchantment's protocol id
    /// and the level it would come at, or -1 for a row with nothing behind it.
    std::array<std::int32_t, 3> enchantmentCosts_{0, 0, 0};
    std::array<std::int32_t, 3> enchantmentClues_{-1, -1, -1};
    std::array<std::int32_t, 3> enchantmentLevels_{-1, -1, -1};
    /// How much power the open table had when its rows were rolled, so taking one works the same
    /// row out again rather than a different one.
    std::int32_t shelvesAround_ = 0;

    bool openGrindstone_ = false;
    std::array<std::optional<storage::StoredItemStack>, 3> grindstoneSlots_;
    /// What the two things in it were worth at a table, which is what grinding them hands back.
    std::int32_t grindstoneWorth_ = 0;

    bool openAnvil_ = false;
    std::array<std::optional<storage::StoredItemStack>, 3> anvilSlots_;
    /// What the player has typed into the name box. Nothing means they have typed nothing, which is
    /// not the same as having emptied it.
    std::optional<std::string> anvilName_;
    std::int32_t anvilCost_ = 0;
    /// How many of the addition a repair is going to eat, which is not always all of them.
    std::int32_t anvilMaterials_ = 0;
    /// parity: `onlyRenaming`, which is what leaves the addition where it is and lifts the wall.
    bool anvilOnlyRenaming_ = false;

    std::array<std::int32_t, 4> openBlockData_{-1, -1, -1, -1};
    std::array<std::optional<storage::StoredItemStack>, 3> shownBlockSlots_;

    /// The click-and-drag gesture in progress, if any.
    bool quickCraftActive_ = false;
    std::int32_t quickCraftType_ = 0;
    std::vector<std::int32_t> quickCraftSlots_;

    std::int32_t lecternPage_ = 0;
};

} // namespace basalt::server::runtime

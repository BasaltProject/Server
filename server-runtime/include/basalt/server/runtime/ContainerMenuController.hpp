#pragma once

#include <basalt/server/plugins/PluginWindow.hpp>
#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/Enchanting.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

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

namespace basalt::server::world {
class ChunkBlockEntity;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class ClientSession;

struct PluginMenuItem {
    std::string id;
    std::int32_t count = 1;
    std::optional<std::string> name;
    std::vector<std::string> lore;
    wire::nbt::NbtCompound components;
};

class ContainerMenuController final : public server::plugins::PluginWindow {
public:
    /// parity: the four grid cells of a player's own inventory, and the nine of a table.
    static constexpr std::int32_t kMaxGridSize = 9;

    struct Hooks {
        std::function<bool(std::string_view kind, const std::optional<wire::protocol::play::BlockPos>& pos)>
            openAllowed;
        std::function<bool(std::int32_t slot, std::int32_t button, std::string_view mode,
                           const std::optional<std::string>& item)>
            clickAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item)>
            pickAllowed;
        std::function<void(std::string_view kind, std::string_view reason)> closed;
        std::function<bool(std::string_view type, std::string_view slots)> dragAllowed;
        std::function<void()> changed;
        std::function<void(std::string_view kind, std::string_view recipe, std::string_view name,
                           std::optional<storage::StoredItemStack>& result, std::int32_t& cost)>
            prepared;
        std::function<bool(std::string_view kind, std::string_view recipe, const storage::StoredItemStack& result,
                           bool shift)>
            takeAllowed;
        /// The three rows an enchanting table is about to show for an item; false hides them all.
        std::function<bool(const storage::StoredItemStack& item, std::int32_t bookshelves,
                           std::array<world::enchanting::Row, 3>& rows)>
            enchantPrepared;
        /// The button pressed, the levels it costs and the enchantments it adds; false stops it.
        std::function<bool(const storage::StoredItemStack& item, std::int32_t button, std::int32_t& cost,
                           std::vector<world::enchanting::Offer>& offers)>
            enchantAllowed;
    };

    struct Options {
        std::function<double()> nextDouble;
        std::function<float()> nextFloat;
        std::function<std::int64_t()> nextSeed;
    };

    explicit ContainerMenuController(ClientSession& owner, Hooks hooks = {}, Options options = {});

    player::Menu openMenu = player::Menu::playerInventory();

    std::int32_t nextContainerId = 1;

    std::optional<wire::protocol::play::BlockPos> openBlock;
    std::optional<wire::protocol::play::BlockPos> openBlockSecond;

    std::int32_t openBlockSize = world::furnaces::kSize;

    std::optional<wire::protocol::play::BlockPos> openLecternAt;

    void openCraftingTable(player::PlayerSession& session);

    void openFurnace(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                     const std::string& cooker);

    void openEnderChest(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// parity: `ContainerEntity.interactWithContainerVehicle`.
    void openEntityChest(player::PlayerSession& session, entity::ServerEntity& vehicle);

    /// parity: `ServerPlayer.openHorseInventory`.
    void openMountMenu(player::PlayerSession& session, entity::ServerEntity& mount);

    void openChest(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, const std::string& block);

    void openSmithingMenu(player::PlayerSession& session);

    void openAnvilMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openGrindstoneMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openEnchantmentMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openBeaconMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openLoomMenu(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openCartographyTable(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openBrewingStand(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// parity: `MerchantMenu`.
    void openMerchantMenu(player::PlayerSession& session);

    [[nodiscard]] bool brewingStandOpen() const;

    /// parity: `BeaconMenu.updateEffects`.
    void setBeaconEffects(const std::optional<std::int32_t>& primary, const std::optional<std::int32_t>& secondary);

    /// parity: `ServerboundRenameItemPacket`.
    void setAnvilName(const std::string& name);

    void openStonecutterMenu(player::PlayerSession& session);

    void openCrafter(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    std::int32_t openWindow(const server::plugins::PluginWindowSpec& spec) override;

    void closeWindow() override;

    [[nodiscard]] server::plugins::PluginWindowInfo windowInfo() const override;

    [[nodiscard]] std::optional<server::plugins::PluginItem> windowItem(std::int32_t slot) const override;

    void setWindowItem(std::int32_t slot, const std::optional<server::plugins::PluginItem>& item) override;

    [[nodiscard]] std::optional<server::plugins::PluginItem> cursorItem() const override;

    void setCursorItem(const std::optional<server::plugins::PluginItem>& item) override;

    void resyncWindow() override { sendInventory(); }

    void openPluginMenu(const std::string& title, std::int32_t rows,
                        const std::map<std::int32_t, PluginMenuItem>& items, std::function<void(std::int32_t)> onClick);

    void refreshPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items);

    void closeOpenMenu();

    void closeOpenMenu(player::PlayerSession& session) { closeOpenMenu(session, "unknown"); }

    void closeOpenMenu(player::PlayerSession& session, std::string_view reason);

    /// parity: `ContainerButtonClick`.
    void clickContainerButton(std::int32_t button);

    /// parity: `handleContainerSlotStateChanged`.
    void setSlotState(std::int32_t slot, bool enabled);

    void syncOpenContainer();

    void sendFurnaceBars();

    /// parity: `BrewingStandBlockEntity.dataAccess`'s four.
    void sendBrewingBars();

    void syncBrewingStand();

    void returnCraftingGrid(player::PlayerSession& session);

    void handleContainerClick(const wire::protocol::play::ServerboundContainerClickPacket& packet);

    void handleContainerClickChecked(const wire::protocol::play::ServerboundContainerClickPacket& packet);

    void placeRecipe(const wire::protocol::play::PlaceRecipePacket& packet);

    void handleSetCreativeModeSlot(const wire::protocol::play::ServerboundSetCreativeModeSlotPacket& packet);

    void handlePickItemFromBlock(const wire::protocol::play::PickItemFromBlockPacket& packet);

    void handlePickItemFromEntity(const wire::protocol::play::PickItemFromEntityPacket& packet);

    void sendInventory();

    void sendInventoryAll();

    void sendInventorySlot(std::int32_t slot);

private:
    [[nodiscard]] std::int32_t allocateContainerId();

    [[nodiscard]] std::string windowKind() const;

    void announceResult(std::string_view kind, std::optional<storage::StoredItemStack>& result, std::int32_t& cost);

    [[nodiscard]] bool resultMayBeTaken(const player::MenuSlot& slot, bool shift) const;

    [[nodiscard]] bool mayOpen(std::string_view kind, const std::optional<wire::protocol::play::BlockPos>& pos) const {
        return !hooks_.openAllowed || hooks_.openAllowed(kind, pos);
    }

    [[nodiscard]] std::optional<storage::StoredItemStack> readBlockSlot(std::int32_t index) const;

    [[nodiscard]] std::optional<storage::StoredItemStack> entityContainerSlot(std::int32_t index) const;
    void writeEntityContainerSlot(std::int32_t index, const std::optional<storage::StoredItemStack>& stack);

    [[nodiscard]] bool mountOpen() const noexcept { return openEntity_.has_value() && openMount_; }

    /// parity: `HorseInventoryMenu`'s `ArmorSlot.mayPlace`, and anything in the chest.
    [[nodiscard]] bool mountAccepts(std::int32_t index, const storage::StoredItemStack& stack) const;

    /// parity: `AbstractMountInventoryMenu.quickMoveStack`.
    void quickMoveMount(player::PlayerSession& session, const player::MenuSlot& slot,
                        const storage::StoredItemStack& stack);

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

    /// parity: `ResultSlot.checkTakeAchievements`.
    void awardRecipeCrafted();

    /// parity: `ServerPlayer.awardRecipes`.
    void awardRecipes(player::PlayerSession& session, const std::vector<wire::common::Identifier>& ids);

    void takeSmithingResult();

    /// parity: `EnchantmentMenu.slotsChanged`.
    void rollEnchantmentOffers();

    /// parity: `EnchantmentMenu.clickMenuButton`, the row they pressed.
    void takeEnchantmentOffer(player::PlayerSession& session, std::int32_t row);

    void sendEnchantmentNumbers();

    /// parity: `BeaconMenu`'s three data slots: the pyramid and the two effects it is set to.
    void sendBeaconNumbers();

    /// parity: `LoomMenu.slotsChanged`.
    void recomputeLoomResult();

    /// parity: `LoomMenu.clickMenuButton`, whose button is a place in the patterns on offer.
    void selectLoomPattern(std::int32_t button);

    void takeLoomResult();

    /// parity: `CartographyTableMenu.setupResultSlot`.
    void recomputeCartographyResult();

    /// parity: the result slot's `onTake`, both inputs spent and the table's sound.
    void takeCartographyResult();

    /// parity: `MapItem.onCraftedPostProcess`.
    void settleCraftedMaps(player::PlayerSession& session);

    void sendLoomNumbers();

    [[nodiscard]] world::beacons::Stored beaconState() const;

    void recomputeGrindstoneResult();

    void takeGrindstoneResult(player::PlayerSession& session);

    void recomputeAnvilResult();

    void takeAnvilResult(player::PlayerSession& session);

    /// parity: `AnvilBlock.damage`.
    void wearAnvil();

    void sendAnvilCost();

    /// parity: `Slot.mayPickup`.
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

    void pickItem(player::PlayerSession& session, const storage::StoredItemStack& picked);

    void announceOpeners(const wire::protocol::play::BlockPos& pos, const std::string& block, std::int32_t openers,
                         std::optional<wire::common::Uuid> opener = std::nullopt);

    void fillPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items);

    [[nodiscard]] std::optional<world::furnaces::State> furnaceState() const;

    [[nodiscard]] std::optional<world::brewingStands::State> brewingState() const;

    /// parity: `BrewingStandMenu.quickMoveStack`.
    void quickMoveIntoBrewingStand(player::PlayerSession& session, const player::MenuSlot& from,
                                   const storage::StoredItemStack& stack);

    /// parity: `PotionSlot.getMaxStackSize`, one, and every other slot's 64.
    [[nodiscard]] std::int32_t slotCapacity(const player::MenuSlot& slot) const;

    /// parity: `Slot.getMaxStackSize(ItemStack)`.
    [[nodiscard]] std::int32_t slotCapacity(const player::MenuSlot& slot, const storage::StoredItemStack& stack) const;

    void payFurnace(world::LoadedWorld& world, server::world::ChunkBlockEntity& entity,
                    const wire::protocol::play::BlockPos& at, const std::string& takenItem, std::int32_t takenAmount);

    /// parity: `BrewingStandMenu.PotionSlot.onTake`'s trigger, for a bottle just taken out.
    void awardBrewed(const server::world::loot::ItemStack& bottle);

    [[nodiscard]] std::optional<std::pair<wire::protocol::play::BlockPos, std::int32_t>>
    halfFor(std::int32_t index) const;

    [[nodiscard]] std::int32_t halfSize() const;

    ClientSession* owner_;
    Hooks hooks_;
    Options options_;

    std::int32_t containerStateId_ = 0;

    std::array<std::optional<storage::StoredItemStack>, kMaxGridSize> craftingGrid_;
    std::optional<storage::StoredItemStack> craftingResult_;
    std::optional<storage::StoredItemStack> carriedItem_;

    bool openEnderChest_ = false;
    std::optional<std::int32_t> openEntity_;
    bool openMount_ = false;
    std::optional<std::vector<std::optional<storage::StoredItemStack>>> pluginMenu_;
    std::function<void(std::int32_t)> pluginMenuClick_;
    std::string pluginKind_;
    std::string closeReason_;
    bool pluginEditable_ = false;
    std::optional<wire::protocol::play::BlockPos> openedAt_;

    bool openStonecutter_ = false;
    std::optional<storage::StoredItemStack> stonecutterInput_;
    std::optional<storage::StoredItemStack> stonecutterResult_;

    bool openSmithing_ = false;
    /// parity: `ResultContainer.recipeUsed`, the recipe that filled the result slot.
    std::optional<wire::common::Identifier> recipeUsed_;
    std::array<std::optional<storage::StoredItemStack>, 4> smithingSlots_;

    bool openEnchantment_ = false;
    std::array<std::optional<storage::StoredItemStack>, 2> enchantmentSlots_;
    std::array<std::int32_t, 3> enchantmentCosts_{0, 0, 0};
    std::array<std::int32_t, 3> enchantmentClues_{-1, -1, -1};
    std::array<world::enchanting::Row, 3> enchantmentRows_;
    std::array<std::int32_t, 3> enchantmentLevels_{-1, -1, -1};
    std::int32_t shelvesAround_ = 0;

    bool openBeacon_ = false;
    std::optional<storage::StoredItemStack> beaconPayment_;

    bool openLoom_ = false;
    std::array<std::optional<storage::StoredItemStack>, 4> loomSlots_;
    /// parity: `selectablePatterns` and `selectedBannerPatternIndex`.
    std::vector<std::string> loomPatterns_;
    std::int32_t loomPattern_ = -1;

    bool openCartography_ = false;
    std::array<std::optional<storage::StoredItemStack>, 3> cartographySlots_;
    std::int64_t cartographySoundTime_ = -1;

    bool openMerchant_ = false;

    bool openGrindstone_ = false;
    std::array<std::optional<storage::StoredItemStack>, 3> grindstoneSlots_;
    std::int32_t grindstoneWorth_ = 0;

    bool openAnvil_ = false;
    std::array<std::optional<storage::StoredItemStack>, 3> anvilSlots_;
    std::optional<std::string> anvilName_;
    std::int32_t anvilCost_ = 0;
    std::int32_t anvilMaterials_ = 0;
    /// parity: `onlyRenaming`.
    bool anvilOnlyRenaming_ = false;

    std::array<std::int32_t, 4> openBlockData_{-1, -1, -1, -1};
    std::array<std::optional<storage::StoredItemStack>, 5> shownBlockSlots_;

    bool quickCraftActive_ = false;
    std::int32_t quickCraftType_ = 0;
    std::vector<std::int32_t> quickCraftSlots_;

    std::int32_t lecternPage_ = 0;
};

} // namespace basalt::server::runtime

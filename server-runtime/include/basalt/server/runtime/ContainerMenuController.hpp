#pragma once

#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

/// One item a plugin drew into a window it opened.
struct PluginMenuItem {
    std::string item;
    std::int32_t count = 1;
    std::optional<std::string> name;
    std::vector<std::string> lore;
};

/// Container, menu, inventory and crafting state: every window this player can have open.
class ContainerMenuController {
public:
    /// What a plugin gets a say in, and what it is told. Unset means nobody objects.
    struct Hooks {
        /// Whether a window may open, given what kind it is and where.
        std::function<bool(std::string_view kind, const std::optional<wire::protocol::play::BlockPos>& pos)>
            openAllowed;
        /// Whether one click may go ahead: the slot, the button and the window.
        std::function<bool(std::int32_t containerId, std::int32_t slot, std::int32_t button)> clickAllowed;
        std::function<void(std::int32_t containerId)> closed;
    };

    explicit ContainerMenuController(ClientSession& owner, Hooks hooks = {});

    /// The window this player has open. Their own inventory unless they opened a block's.
    player::Menu openMenu = player::Menu::playerInventory();

    /// The last container id handed out, so a freshly opened window never reuses the previous one.
    std::int32_t nextContainerId = 1;

    /// The block whose window is open, if the open window belongs to one.
    std::optional<wire::protocol::play::BlockPos> openBlock;

    /// The other half of an open double chest, whose slots follow the first half's.
    std::optional<wire::protocol::play::BlockPos> openBlockSecond;

    /// How many slots the open block has, which is what a click is bounds-checked against.
    std::int32_t openBlockSize = 3;

    /// The lectern whose page this player is turning, if that is what is open.
    std::optional<wire::protocol::play::BlockPos> openLecternAt;

    void openCraftingTable(player::PlayerSession& session);

    void openFurnace(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                     const std::string& cooker);

    void openEnderChest(player::PlayerSession& session);

    void openChest(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openSmithingMenu(player::PlayerSession& session);

    void openStonecutterMenu(player::PlayerSession& session);

    void openCrafter(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// Opens a window a plugin drew. The plugin bridge's entry point, so the shape stays exact.
    void openPluginMenu(const std::string& title, std::int32_t rows,
                        const std::map<std::int32_t, PluginMenuItem>& items, std::function<void(std::int32_t)> onClick);

    /// Puts new contents in the open menu and redraws it.
    void refreshPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items);

    /// Shuts whatever window is open. Also the plugin bridge's exit.
    void closeOpenMenu();

    void closeOpenMenu(player::PlayerSession& session);

    /// parity: `ContainerButtonClick`, which is how a lectern's page and a loom's pattern arrive.
    void clickContainerButton(std::int32_t button);

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

    /// The whole inventory, and which slot is in hand.
    void sendInventory();

    /// Re-sends the whole window, for a change made on the server side.
    void sendInventoryAll();

    /// One slot changed.
    void sendInventorySlot(std::int32_t slot);

private:
    /// parity: `CraftingMenu.slotsChanged`, which is what puts the result in the output slot.
    void recomputeCraftingResult();

    /// The next container id, wrapping rather than growing without end.
    [[nodiscard]] std::int32_t allocateContainerId();

    ClientSession* owner_;
    Hooks hooks_;
    /// What a plugin drew, and what to call when one of its slots is clicked.
    std::map<std::int32_t, PluginMenuItem> pluginItems_;
    std::function<void(std::int32_t)> pluginClick_;
    /// What the player is dragging, which is a click's worth of state between two packets.
    std::optional<storage::StoredItemStack> carried_;
};

} // namespace basalt::server::runtime

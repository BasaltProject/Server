package org.basaltmc.bukkit.inv;

import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.Items;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.InventoryView;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.MenuType;
import org.bukkit.inventory.PlayerInventory;

/** One player's open window: the inventory above, their own below, and the item on their cursor. */
public class BasaltInventoryView implements InventoryView {
    protected final HumanEntity player;
    protected final Inventory top;
    protected final PlayerInventory bottom;
    protected volatile Component title;
    protected final boolean playerWindow;
    protected final int containerId;

    public BasaltInventoryView(HumanEntity player, Inventory top, Component title, boolean playerWindow,
            int containerId) {
        this.player = player;
        this.top = top;
        this.bottom = player.getInventory();
        this.title = title;
        this.playerWindow = playerWindow;
        this.containerId = containerId;
    }

    public int containerId() {
        return containerId;
    }

    public boolean playerWindow() {
        return playerWindow;
    }

    private String uuid() {
        return player.getUniqueId().toString();
    }

    @Override
    public Inventory getTopInventory() {
        return top;
    }

    @Override
    public Inventory getBottomInventory() {
        return bottom;
    }

    @Override
    public HumanEntity getPlayer() {
        return player;
    }

    @Override
    public InventoryType getType() {
        return top.getType();
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        if (slot == OUTSIDE) {
            if (item != null) {
                player.getWorld().dropItem(player.getLocation(), item);
            }
            return;
        }
        if (slot < top.getSize()) {
            top.setItem(slot, item);
        } else {
            bottom.setItem(convertSlot(slot), item);
        }
    }

    @Override
    public ItemStack getItem(int slot) {
        if (slot == OUTSIDE || slot < 0) {
            return null;
        }
        if (slot < top.getSize()) {
            return top.getItem(slot);
        }
        int converted = convertSlot(slot);
        return converted < 0 ? null : bottom.getItem(converted);
    }

    @Override
    public void setCursor(ItemStack item) {
        InventoryNatives.setCursorItem(uuid(), Items.toBytes(item));
    }

    @Override
    public ItemStack getCursor() {
        ItemStack cursor = Items.fromBytes(InventoryNatives.cursorItem(uuid()));
        return cursor == null ? ItemStack.empty() : cursor;
    }

    @Override
    public Inventory getInventory(int rawSlot) {
        if (rawSlot == OUTSIDE) {
            return null;
        }
        if (rawSlot < 0) {
            throw new IllegalArgumentException("Negative, non outside slot " + rawSlot);
        }
        if (rawSlot >= countSlots()) {
            throw new IllegalArgumentException("Slot " + rawSlot + " greater than inventory slot count");
        }
        return rawSlot < top.getSize() ? top : bottom;
    }

    @Override
    public int convertSlot(int rawSlot) {
        int topSize = top.getSize();
        if (rawSlot < topSize) {
            return rawSlot;
        }
        int offset = rawSlot - topSize;
        if (playerWindow) {
            if (offset < 4) {
                return 39 - offset;
            }
            if (offset < 31) {
                return offset - 4 + 9;
            }
            if (offset < 40) {
                return offset - 31;
            }
            return offset == 40 ? 40 : -1;
        }
        if (offset < 27) {
            return offset + 9;
        }
        return offset < 36 ? offset - 27 : -1;
    }

    @Override
    public InventoryType.SlotType getSlotType(int slot) {
        if (slot == OUTSIDE) {
            return InventoryType.SlotType.OUTSIDE;
        }
        if (slot < 0) {
            return InventoryType.SlotType.OUTSIDE;
        }
        int topSize = top.getSize();
        if (slot < topSize) {
            return InventoryTypes.topSlotType(top.getType(), slot);
        }
        int converted = convertSlot(slot);
        if (converted >= 36 && converted <= 39) {
            return InventoryType.SlotType.ARMOR;
        }
        return converted >= 0 && converted < 9 || converted == 40 ? InventoryType.SlotType.QUICKBAR
            : InventoryType.SlotType.CONTAINER;
    }

    @Override
    public void open() {
        Windows.open(player, this);
    }

    @Override
    public void close() {
        player.closeInventory();
    }

    @Override
    public int countSlots() {
        return top.getSize() + (playerWindow ? 41 : 36);
    }

    @Override
    @SuppressWarnings("removal")
    public boolean setProperty(Property prop, int value) {
        return org.basaltmc.bukkit.LeftoverNatives.windowProperty(player.getUniqueId().toString(), prop.getId(), value);
    }

    @Override
    public Component title() {
        return title;
    }

    @Override
    public String getTitle() {
        return org.basaltmc.bukkit.item.Texts.toLegacy(title);
    }

    @Override
    public String getOriginalTitle() {
        return getTitle();
    }

    @Override
    public void setTitle(String title) {
        Component next = net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer.legacySection()
            .deserialize(title);
        if (org.basaltmc.bukkit.player.PlayerCalls.done(player.getUniqueId(), "window_title",
            org.basaltmc.bukkit.misc.Json.of(next))) {
            this.title = next;
        }
    }

    @Override
    public MenuType getMenuType() {
        return top.getType().getMenuType();
    }
}

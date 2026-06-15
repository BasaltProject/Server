package org.basaltmc.bukkit.inv;

import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.InventoryHolder;

/** What `Bukkit.createInventory` builds. */
public final class Creation {
    private Creation() {}

    public static Inventory of(InventoryHolder holder, InventoryType type, Component title) {
        if (type == null) {
            throw new IllegalArgumentException("The InventoryType cannot be null");
        }
        if (!type.isCreatable()) {
            throw new IllegalArgumentException("Cannot open an inventory of type " + type);
        }
        return TypedInventories.wrap(new CustomInventory(holder, type, type.getDefaultSize(), title));
    }

    public static Inventory of(InventoryHolder holder, int size, Component title) {
        if (size < 9 || size > 54 || size % 9 != 0) {
            throw new IllegalArgumentException("Size for custom inventory must be a multiple of 9 between 9 and 54 slots (got " + size + ")");
        }
        return new CustomInventory(holder, InventoryType.CHEST, size, title);
    }

    public static Inventory of(InventoryHolder holder, InventoryType type, String title) {
        return of(holder, type, title == null ? null : Texts.fromLegacy(title));
    }

    public static Inventory of(InventoryHolder holder, int size, String title) {
        return of(holder, size, title == null ? null : Texts.fromLegacy(title));
    }
}

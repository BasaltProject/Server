package org.basaltmc.bukkit.entity.ops;

import java.util.Locale;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ItemDisplay;
import org.bukkit.inventory.ItemStack;

public interface ItemDisplayOps extends ItemDisplay, Cored {
    default ItemStack getItemStack() {
        return EntityNbt.item(core(), "item", null);
    }

    default void setItemStack(ItemStack item) {
        EntityNbt.storeItem(core(), "item", item);
    }

    default ItemDisplay.ItemDisplayTransform getItemDisplayTransform() {
        String name = core().nbtText("item_display");
        return name == null ? ItemDisplay.ItemDisplayTransform.NONE
            : ItemDisplay.ItemDisplayTransform.valueOf(name.toUpperCase(Locale.ROOT));
    }

    default void setItemDisplayTransform(ItemDisplay.ItemDisplayTransform transform) {
        if (transform == null) {
            throw new IllegalArgumentException("Display transform cannot be null");
        }
        core().nbtSet("item_display", "string", transform.name().toLowerCase(Locale.ROOT));
    }
}

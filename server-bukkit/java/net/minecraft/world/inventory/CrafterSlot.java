// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.item.ItemStack;

public class CrafterSlot extends Slot {
    private final CrafterMenu menu;

    public CrafterSlot(Container container, int slot, int x, int y, CrafterMenu menu) {
        super(container, slot, x, y);
        this.menu = menu;
    }

    public boolean mayPlace(ItemStack itemStack) {
        return !menu.isSlotDisabled(index) && super.mayPlace(itemStack);
    }

    public void setChanged() {
        super.setChanged();
        menu.slotsChanged(container);
    }
}

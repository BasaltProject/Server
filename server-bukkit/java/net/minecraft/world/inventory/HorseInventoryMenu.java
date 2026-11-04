// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.entity.animal.equine.AbstractHorse;
import net.minecraft.world.entity.player.Inventory;

public class HorseInventoryMenu extends AbstractMountInventoryMenu {
    private final AbstractHorse horse;

    public HorseInventoryMenu(int containerId, Inventory playerInventory, Container horseInventory, AbstractHorse horse, int inventoryColumns) {
        super(containerId, playerInventory, horseInventory, horse);
        this.horse = horse;
    }

    @Override
    protected boolean hasInventoryChanged(Container container) {
        return horse.hasInventoryChanged(container);
    }
}

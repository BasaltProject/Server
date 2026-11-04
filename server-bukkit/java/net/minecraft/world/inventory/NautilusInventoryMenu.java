// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.entity.animal.nautilus.AbstractNautilus;
import net.minecraft.world.entity.player.Inventory;

public class NautilusInventoryMenu extends AbstractMountInventoryMenu {
    private final AbstractNautilus nautilus;

    public NautilusInventoryMenu(int containerId, Inventory playerInventory, Container nautilusInventory, AbstractNautilus nautilus, int inventoryColumns) {
        super(containerId, playerInventory, nautilusInventory, nautilus);
        this.nautilus = nautilus;
    }

    @Override
    protected boolean hasInventoryChanged(Container container) {
        return nautilus.isRemoved();
    }
}

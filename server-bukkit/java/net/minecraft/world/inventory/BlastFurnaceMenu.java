// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class BlastFurnaceMenu extends AbstractFurnaceMenu {
    public BlastFurnaceMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, new SimpleContainer(SLOT_COUNT), new SimpleContainerData(DATA_COUNT));
    }

    public BlastFurnaceMenu(int containerId, Inventory inventory, Container container, ContainerData data) {
        super(MenuType.BLAST_FURNACE, containerId, inventory, container, data);
    }

    @Override
    protected String recipeKind() {
        return "blasting";
    }
}

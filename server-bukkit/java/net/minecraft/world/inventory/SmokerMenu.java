// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class SmokerMenu extends AbstractFurnaceMenu {
    public SmokerMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, new SimpleContainer(SLOT_COUNT), new SimpleContainerData(DATA_COUNT));
    }

    public SmokerMenu(int containerId, Inventory inventory, Container container, ContainerData data) {
        super(MenuType.SMOKER, containerId, inventory, container, data);
    }

    @Override
    protected String recipeKind() {
        return "smoking";
    }
}

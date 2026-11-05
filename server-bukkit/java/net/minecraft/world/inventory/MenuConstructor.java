// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
public interface MenuConstructor {
    AbstractContainerMenu createMenu(int containerId, Inventory inventory, Player player);
}

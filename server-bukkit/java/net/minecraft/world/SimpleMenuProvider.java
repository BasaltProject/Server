// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.inventory.MenuConstructor;
public final class SimpleMenuProvider implements MenuProvider {
    private final Component title;
    private final MenuConstructor factory;
    public SimpleMenuProvider(MenuConstructor menuConstructor, Component title) {
        this.factory = menuConstructor;
        this.title = title;
    }
    public Component getDisplayName() {
        return title;
    }
    public AbstractContainerMenu createMenu(int containerId, Inventory inventory, Player player) {
        return factory.createMenu(containerId, inventory, player);
    }
}

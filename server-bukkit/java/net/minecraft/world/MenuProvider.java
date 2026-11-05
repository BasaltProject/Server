// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import net.minecraft.world.inventory.MenuConstructor;
import net.minecraft.network.chat.Component;
public interface MenuProvider extends MenuConstructor {
    Component getDisplayName();
}

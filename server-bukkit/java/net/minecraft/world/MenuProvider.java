// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import net.minecraft.world.inventory.MenuConstructor;
import net.minecraft.network.chat.Component;
public interface MenuProvider extends MenuConstructor {
    default Component getDisplayName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.MenuProvider#getDisplayName");
    }
}

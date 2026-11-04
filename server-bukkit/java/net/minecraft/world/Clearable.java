// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import org.basaltmc.bukkit.Unsupported;
public interface Clearable {
    public default void clearContent() {
        throw Unsupported.hit("net.minecraft.world.Clearable.clearContent");
    }
}

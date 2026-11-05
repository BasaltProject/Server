// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import net.minecraft.network.chat.Component;
public interface Nameable {
    Component getName();
    default String getPlainTextName() {
        return getName().getString();
    }
    default boolean hasCustomName() {
        return getCustomName() != null;
    }
    default Component getDisplayName() {
        return getName();
    }
    default Component getCustomName() {
        return null;
    }
}

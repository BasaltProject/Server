// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import net.minecraft.network.chat.Component;
public interface Nameable {
    default Component getName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.Nameable#getName");
    }
    default String getPlainTextName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.Nameable#getPlainTextName");
    }
    default boolean hasCustomName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.Nameable#hasCustomName");
    }
    default Component getDisplayName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.Nameable#getDisplayName");
    }
    default Component getCustomName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.Nameable#getCustomName");
    }
}

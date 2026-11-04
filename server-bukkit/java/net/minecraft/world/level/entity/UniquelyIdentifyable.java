// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;

import java.util.UUID;
public interface UniquelyIdentifyable {
    default UUID getUUID() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.UniquelyIdentifyable#getUUID");
    }
    default boolean isRemoved() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.UniquelyIdentifyable#isRemoved");
    }
}

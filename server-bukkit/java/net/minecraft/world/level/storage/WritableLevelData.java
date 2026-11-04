// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
public interface WritableLevelData extends LevelData {
    default void setSpawn(LevelData.RespawnData respawnData) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.WritableLevelData#setSpawn");
    }
}

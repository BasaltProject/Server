package org.basaltmc.bukkit.persistence;

import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer;

/** The containers of the holders the server stores itself; each reads and writes the server's own copy. */
public final class PdcStores {
    private PdcStores() {}

    public static CraftPersistentDataContainer world(String world) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> MiscNatives.worldData(world),
            bytes -> MiscNatives.setWorldData(world, bytes)));
    }

    public static CraftPersistentDataContainer chunk(String world, int chunkX, int chunkZ) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> MiscNatives.chunkData(world, chunkX, chunkZ),
            bytes -> MiscNatives.setChunkData(world, chunkX, chunkZ, bytes)));
    }

    public static CraftPersistentDataContainer blockEntity(String world, int x, int y, int z) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> MiscNatives.blockEntityData(world, x, y, z),
            bytes -> MiscNatives.setBlockEntityData(world, x, y, z, bytes)));
    }

    public static CraftPersistentDataContainer entity(String world, int entityId) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> MiscNatives.entityData(world, entityId),
            bytes -> MiscNatives.setEntityData(world, entityId, bytes)));
    }

    public static CraftPersistentDataContainer player(String uuid) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> MiscNatives.playerData(uuid),
            bytes -> MiscNatives.setPlayerData(uuid, bytes)));
    }
}

package org.basaltmc.bukkit.persistence;

import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.MiscNatives;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer;

public final class PdcStores {
    private PdcStores() {}

    public static CraftPersistentDataContainer world(String world) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> MiscNatives.worldData(world),
            bytes -> MiscNatives.setWorldData(world, bytes)));
    }

    public static CraftPersistentDataContainer chunk(String world, int chunkX, int chunkZ) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> {
            byte[] found = MiscNatives.chunkData(world, chunkX, chunkZ);
            if (found == null && WorldNatives.loadChunk(world, chunkX, chunkZ, true)) {
                found = MiscNatives.chunkData(world, chunkX, chunkZ);
            }
            return found;
        }, bytes -> MiscNatives.setChunkData(world, chunkX, chunkZ, bytes)));
    }

    public static CraftPersistentDataContainer blockEntity(String world, int x, int y, int z) {
        return new CraftPersistentDataContainer(new BytesPdcStore(() -> {
            Blocks.flushAll();
            return MiscNatives.blockEntityData(world, x, y, z);
        }, bytes -> {
            Blocks.flushAll();
            MiscNatives.setBlockEntityData(world, x, y, z, bytes);
        }));
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

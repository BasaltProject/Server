package org.bukkit.craftbukkit;

import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import org.basaltmc.bukkit.Unsupported;
public class CraftChunk {
    protected CraftChunk() {
    }
    public ChunkAccess getHandle(ChunkStatus chunkStatus) {
        throw Unsupported.hit("org.bukkit.craftbukkit.CraftChunk.getHandle");
    }
}

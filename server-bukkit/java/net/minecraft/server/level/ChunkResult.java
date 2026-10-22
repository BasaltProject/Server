package net.minecraft.server.level;

import org.basaltmc.bukkit.Unsupported;
public interface ChunkResult<T> {
    public default Object orElse(Object orElse) {
        throw Unsupported.hit("net.minecraft.server.level.ChunkResult.orElse");
    }
}

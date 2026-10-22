package net.minecraft.server.level;

import org.basaltmc.bukkit.Unsupported;
public enum FullChunkStatus {
    INACCESSIBLE, FULL, BLOCK_TICKING, ENTITY_TICKING;
    public boolean isOrAfter(FullChunkStatus step) {
        throw Unsupported.hit("net.minecraft.server.level.FullChunkStatus.isOrAfter");
    }
}

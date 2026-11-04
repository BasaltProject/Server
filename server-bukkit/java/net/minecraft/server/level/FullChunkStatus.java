// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

public enum FullChunkStatus {
    INACCESSIBLE, FULL, BLOCK_TICKING, ENTITY_TICKING;

    public boolean isOrAfter(FullChunkStatus step) {
        return ordinal() >= step.ordinal();
    }
}

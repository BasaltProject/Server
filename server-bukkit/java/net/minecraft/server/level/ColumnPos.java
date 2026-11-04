// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.world.level.ChunkPos;

public record ColumnPos(int x, int z) {
    public ChunkPos toChunkPos() {
        return new ChunkPos(x >> 4, z >> 4);
    }

    public long toLong() {
        return asLong(x, z);
    }

    public static long asLong(int x, int z) {
        return (x & 0xFFFFFFFFL) | (z & 0xFFFFFFFFL) << 32;
    }

    public static int getX(long pos) {
        return (int) (pos & 0xFFFFFFFFL);
    }

    public static int getZ(long pos) {
        return (int) (pos >>> 32 & 0xFFFFFFFFL);
    }
}

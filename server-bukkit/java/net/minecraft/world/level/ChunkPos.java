package net.minecraft.world.level;

import java.util.stream.Stream;
import net.minecraft.core.BlockPos;

public record ChunkPos(int x, int z) {
    public static ChunkPos containing(BlockPos pos) {
        return new ChunkPos(pos.getX() >> 4, pos.getZ() >> 4);
    }

    public static long pack(int x, int z) {
        return (long) x & 0xFFFFFFFFL | ((long) z & 0xFFFFFFFFL) << 32;
    }

    public static ChunkPos unpack(long key) {
        return new ChunkPos((int) key, (int) (key >> 32));
    }

    public long pack() {
        return pack(x, z);
    }

    public static Stream<ChunkPos> rangeClosed(ChunkPos from, ChunkPos to) {
        int width = to.x - from.x + 1;
        int depth = to.z - from.z + 1;
        return Stream.iterate(0, i -> i + 1).limit((long) width * depth)
            .map(i -> new ChunkPos(from.x + i % width, from.z + i / width));
    }

    public int getMinBlockX() {
        return x << 4;
    }

    public int getMinBlockZ() {
        return z << 4;
    }

    public int getMaxBlockX() {
        return (x << 4) + 15;
    }

    public int getMaxBlockZ() {
        return (z << 4) + 15;
    }
}

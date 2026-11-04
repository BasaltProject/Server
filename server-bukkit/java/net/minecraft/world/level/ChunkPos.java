// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;

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
    public static final Codec<ChunkPos> CODEC = Codec.INT_STREAM.comapFlatMap(
        stream -> net.minecraft.util.Util.fixedSize(stream, 2).map(a -> new ChunkPos(a[0], a[1])),
        pos -> java.util.stream.IntStream.of(pos.x, pos.z)).stable();
    public static final StreamCodec<ByteBuf, ChunkPos> STREAM_CODEC = StreamCodec.of(
        (buf, pos) -> buf.writeLong(pos.pack()), buf -> unpack(buf.readLong()));
    public static final long INVALID_CHUNK_POS = pack(1875066, 1875066);
    public static final ChunkPos ZERO = new ChunkPos(0, 0);
    public static final int REGION_SIZE = 32;
    public static final int REGION_MAX_INDEX = 31;
    public static ChunkPos minFromRegion(int regionX, int regionZ) {
        return new ChunkPos(regionX << 5, regionZ << 5);
    }
    public static ChunkPos maxFromRegion(int regionX, int regionZ) {
        return new ChunkPos((regionX << 5) + 31, (regionZ << 5) + 31);
    }
    public boolean isValid() {
        return isValid(x, z);
    }
    public static boolean isValid(int x, int z) {
        return Math.abs(x) <= 1875066 && Math.abs(z) <= 1875066;
    }
    public static long fromSectionNode(long sectionNode) {
        return pack(net.minecraft.core.SectionPos.x(sectionNode), net.minecraft.core.SectionPos.z(sectionNode));
    }
    public static long pack(BlockPos pos) {
        return pack(pos.getX() >> 4, pos.getZ() >> 4);
    }
    public static int getX(long pos) {
        return (int) (pos & 0xFFFFFFFFL);
    }
    public static int getZ(long pos) {
        return (int) (pos >>> 32 & 0xFFFFFFFFL);
    }
    public int hashCode() {
        return hash(x, z);
    }
    public static int hash(int x, int z) {
        return (1664525 * x + 1013904223) ^ (1664525 * (z ^ -559038737) + 1013904223);
    }
    public int getMiddleBlockX() {
        return getBlockX(8);
    }
    public int getMiddleBlockZ() {
        return getBlockZ(8);
    }
    public int getRegionX() {
        return x >> 5;
    }
    public int getRegionZ() {
        return z >> 5;
    }
    public static int getRegionX(long pos) {
        return getX(pos) >> 5;
    }
    public static int getRegionZ(long pos) {
        return getZ(pos) >> 5;
    }
    public int getRegionLocalX() {
        return x & 31;
    }
    public int getRegionLocalZ() {
        return z & 31;
    }
    public BlockPos getBlockAt(int x, int y, int z) {
        return new BlockPos(getBlockX(x), y, getBlockZ(z));
    }
    public int getBlockX(int offset) {
        return (x << 4) + offset;
    }
    public int getBlockZ(int offset) {
        return (z << 4) + offset;
    }
    public BlockPos getMiddleBlockPosition(int y) {
        return new BlockPos(getMiddleBlockX(), y, getMiddleBlockZ());
    }
    public boolean contains(BlockPos pos) {
        return pos.getX() >> 4 == x && pos.getZ() >> 4 == z;
    }
    public String toString() {
        return "[" + x + ", " + z + "]";
    }
    public BlockPos getWorldPosition() {
        return new BlockPos(getMinBlockX(), 0, getMinBlockZ());
    }
    public int getChessboardDistance(ChunkPos pos) {
        return getChessboardDistance(pos.x, pos.z);
    }
    public int getChessboardDistance(int x, int z) {
        return Math.max(Math.abs(x - this.x), Math.abs(z - this.z));
    }
    public int distanceSquared(ChunkPos pos) {
        return distanceSquared(pos.x, pos.z);
    }
    private int distanceSquared(int ox, int oz) {
        int dx = ox - x;
        int dz = oz - z;
        return dx * dx + dz * dz;
    }
    public int distanceSquared(long pos) {
        return distanceSquared(getX(pos), getZ(pos));
    }
    public static Stream<ChunkPos> rangeClosed(ChunkPos center, int radius) {
        return rangeClosed(new ChunkPos(center.x - radius, center.z - radius), new ChunkPos(center.x + radius, center.z + radius));
    }
}

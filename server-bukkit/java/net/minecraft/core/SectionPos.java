// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import net.minecraft.util.Mth;
import net.minecraft.world.level.ChunkPos;
import io.netty.buffer.ByteBuf;
import it.unimi.dsi.fastutil.longs.LongConsumer;
import java.util.stream.Stream;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.entity.EntityAccess;

public class SectionPos extends Vec3i {
    protected SectionPos(int x, int y, int z) {
        super(x, y, z);
    }

    public static SectionPos of(int x, int y, int z) {
        return new SectionPos(x, y, z);
    }

    public static SectionPos of(ChunkPos pos, int sectionY) {
        return new SectionPos(pos.x(), sectionY, pos.z());
    }

    public static int blockToSectionCoord(int blockCoord) {
        return blockCoord >> 4;
    }

    public static int sectionToBlockCoord(int sectionCoord) {
        return sectionCoord << 4;
    }
    public static final int SECTION_BITS = 4;
    public static final int SECTION_SIZE = 16;
    public static final int SECTION_BLOCK_COUNT = 4096;
    public static final int SECTION_MASK = 15;
    public static final int SECTION_HALF_SIZE = 8;
    public static final int SECTION_MAX_INDEX = 15;
    public static final StreamCodec<ByteBuf, SectionPos> STREAM_CODEC =
        net.minecraft.network.codec.ByteBufCodecs.LONG.map(SectionPos::of, SectionPos::asLong);
    public static SectionPos of(BlockPos pos) {
        return new SectionPos(blockToSectionCoord(pos.getX()), blockToSectionCoord(pos.getY()), blockToSectionCoord(pos.getZ()));
    }
    public static SectionPos of(EntityAccess entity) {
        return of(entity.blockPosition());
    }
    public static SectionPos of(Position pos) {
        return new SectionPos(blockToSectionCoord(pos.x()), blockToSectionCoord(pos.y()), blockToSectionCoord(pos.z()));
    }
    public static SectionPos of(long sectionNode) {
        return new SectionPos(x(sectionNode), y(sectionNode), z(sectionNode));
    }
    public static SectionPos bottomOf(ChunkAccess chunk) {
        return of(chunk.getPos(), chunk.getMinSectionY());
    }
    public static long offset(long sectionNode, Direction offset) {
        return offset(sectionNode, offset.getStepX(), offset.getStepY(), offset.getStepZ());
    }
    public static long offset(long sectionNode, int stepX, int stepY, int stepZ) {
        return asLong(x(sectionNode) + stepX, y(sectionNode) + stepY, z(sectionNode) + stepZ);
    }
    public static int posToSectionCoord(double pos) {
        return blockToSectionCoord(Mth.floor(pos));
    }
    public static int blockToSectionCoord(double coord) {
        return blockToSectionCoord(Mth.floor(coord));
    }
    public static int sectionRelative(int blockCoord) {
        return blockCoord & 15;
    }
    public static short sectionRelativePos(BlockPos pos) {
        return (short) (sectionRelative(pos.getX()) << 8 | sectionRelative(pos.getZ()) << 4 | sectionRelative(pos.getY()));
    }
    public static int sectionRelativeX(short relative) {
        return relative >>> 8 & 15;
    }
    public static int sectionRelativeY(short relative) {
        return relative & 15;
    }
    public static int sectionRelativeZ(short relative) {
        return relative >>> 4 & 15;
    }
    public int relativeToBlockX(short relative) {
        return minBlockX() + sectionRelativeX(relative);
    }
    public int relativeToBlockY(short relative) {
        return minBlockY() + sectionRelativeY(relative);
    }
    public int relativeToBlockZ(short relative) {
        return minBlockZ() + sectionRelativeZ(relative);
    }
    public BlockPos relativeToBlockPos(short relative) {
        return new BlockPos(relativeToBlockX(relative), relativeToBlockY(relative), relativeToBlockZ(relative));
    }
    public static int sectionToBlockCoord(int sectionCoord, int offset) {
        return (sectionCoord << 4) + offset;
    }
    public static int x(long sectionNode) {
        return (int) (sectionNode >> 42);
    }
    public static int y(long sectionNode) {
        return (int) (sectionNode << 44 >> 44);
    }
    public static int z(long sectionNode) {
        return (int) (sectionNode << 22 >> 42);
    }
    public int x() {
        return getX();
    }
    public int y() {
        return getY();
    }
    public int z() {
        return getZ();
    }
    public int minBlockX() {
        return sectionToBlockCoord(x());
    }
    public int minBlockY() {
        return sectionToBlockCoord(y());
    }
    public int minBlockZ() {
        return sectionToBlockCoord(z());
    }
    public int maxBlockX() {
        return sectionToBlockCoord(x(), 15);
    }
    public int maxBlockY() {
        return sectionToBlockCoord(y(), 15);
    }
    public int maxBlockZ() {
        return sectionToBlockCoord(z(), 15);
    }
    public static long blockToSection(long blockNode) {
        return asLong(blockToSectionCoord(BlockPos.getX(blockNode)), blockToSectionCoord(BlockPos.getY(blockNode)), blockToSectionCoord(BlockPos.getZ(blockNode)));
    }
    public static long getZeroNode(int x, int z) {
        return getZeroNode(asLong(x, 0, z));
    }
    public static long getZeroNode(long sectionNode) {
        return sectionNode & -1048576L;
    }
    public static long sectionToChunk(long sectionNode) {
        return ChunkPos.pack(x(sectionNode), z(sectionNode));
    }
    public BlockPos origin() {
        return new BlockPos(minBlockX(), minBlockY(), minBlockZ());
    }
    public BlockPos center() {
        return origin().offset(8, 8, 8);
    }
    public ChunkPos chunk() {
        return new ChunkPos(x(), z());
    }
    public static long asLong(BlockPos pos) {
        return asLong(blockToSectionCoord(pos.getX()), blockToSectionCoord(pos.getY()), blockToSectionCoord(pos.getZ()));
    }
    public static long asLong(int x, int y, int z) {
        return ((long) x & 4194303L) << 42 | (long) y & 1048575L | ((long) z & 4194303L) << 20;
    }
    public long asLong() {
        return asLong(x(), y(), z());
    }
    public SectionPos offset(int x, int y, int z) {
        return x == 0 && y == 0 && z == 0 ? this : new SectionPos(x() + x, y() + y, z() + z);
    }
    public Stream<BlockPos> blocksInside() {
        return BlockPos.betweenClosedStream(minBlockX(), minBlockY(), minBlockZ(), maxBlockX(), maxBlockY(), maxBlockZ());
    }
    public static Stream<SectionPos> cube(SectionPos center, int radius) {
        return betweenClosedStream(center.x() - radius, center.y() - radius, center.z() - radius, center.x() + radius, center.y() + radius, center.z() + radius);
    }
    public static Stream<SectionPos> aroundChunk(ChunkPos center, int radius, int minSection, int maxSection) {
        return betweenClosedStream(center.x() - radius, minSection, center.z() - radius, center.x() + radius, maxSection, center.z() + radius);
    }
    public static Stream<SectionPos> betweenClosedStream(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        return BlockPos.betweenClosedStream(minX, minY, minZ, maxX, maxY, maxZ).map(pos -> new SectionPos(pos.getX(), pos.getY(), pos.getZ()));
    }
    public static void aroundAndAtBlockPos(BlockPos blockPos, LongConsumer sectionConsumer) {
        aroundAndAtBlockPos(blockPos.getX(), blockPos.getY(), blockPos.getZ(), sectionConsumer);
    }
    public static void aroundAndAtBlockPos(long blockPos, LongConsumer sectionConsumer) {
        aroundAndAtBlockPos(BlockPos.getX(blockPos), BlockPos.getY(blockPos), BlockPos.getZ(blockPos), sectionConsumer);
    }
    public static void aroundAndAtBlockPos(int blockX, int blockY, int blockZ, LongConsumer sectionConsumer) {
        int minX = blockToSectionCoord(blockX - 1);
        int maxX = blockToSectionCoord(blockX + 1);
        int minY = blockToSectionCoord(blockY - 1);
        int maxY = blockToSectionCoord(blockY + 1);
        int minZ = blockToSectionCoord(blockZ - 1);
        int maxZ = blockToSectionCoord(blockZ + 1);
        for (int x = minX; x <= maxX; x++) {
            for (int y = minY; y <= maxY; y++) {
        for (int z = minZ; z <= maxZ; z++) {
            sectionConsumer.accept(asLong(x, y, z));
        }
            }
        }
    }
}

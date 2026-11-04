// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.ChunkPos;

public class BoundingBox {
    public static final Codec<BoundingBox> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.BoundingBox.class, "CODEC");
    public static final StreamCodec<ByteBuf, BoundingBox> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.levelgen.structure.BoundingBox.class, "STREAM_CODEC");
    private int minX;
    private int minY;
    private int minZ;
    private int maxX;
    private int maxY;
    private int maxZ;

    public BoundingBox(BlockPos content) {
        this(content.getX(), content.getY(), content.getZ(), content.getX(), content.getY(), content.getZ());
    }

    public BoundingBox(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        this.minX = minX;
        this.minY = minY;
        this.minZ = minZ;
        this.maxX = maxX;
        this.maxY = maxY;
        this.maxZ = maxZ;
        if (maxX < minX || maxY < minY || maxZ < minZ) {
            throw new IllegalStateException("Invalid bounding box data, inconsistent bounds");
        }
    }

    public static BoundingBox fromCorners(Vec3i a, Vec3i b) {
        return new BoundingBox(Math.min(a.getX(), b.getX()), Math.min(a.getY(), b.getY()), Math.min(a.getZ(), b.getZ()),
            Math.max(a.getX(), b.getX()), Math.max(a.getY(), b.getY()), Math.max(a.getZ(), b.getZ()));
    }

    public static BoundingBox infinite() {
        return new BoundingBox(Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE,
            Integer.MAX_VALUE, Integer.MAX_VALUE);
    }

    public static BoundingBox orientBox(int footX, int footY, int footZ, int offX, int offY, int offZ, int width,
            int height, int depth, Direction direction) {
        if (direction.getAxis() == Direction.Axis.Z) {
            return direction == Direction.NORTH
                ? new BoundingBox(footX + offX, footY + offY, footZ - depth + 1 + offZ, footX + width - 1 + offX,
                    footY + height - 1 + offY, footZ + offZ)
                : new BoundingBox(footX + offX, footY + offY, footZ + offZ, footX + width - 1 + offX,
                    footY + height - 1 + offY, footZ + depth - 1 + offZ);
        }
        return direction == Direction.WEST
            ? new BoundingBox(footX - depth + 1 + offZ, footY + offY, footZ + offX, footX + offZ,
                footY + height - 1 + offY, footZ + width - 1 + offX)
            : new BoundingBox(footX + offZ, footY + offY, footZ + offX, footX + depth - 1 + offZ,
                footY + height - 1 + offY, footZ + width - 1 + offX);
    }

    public Stream<ChunkPos> intersectingChunks() {
        return ChunkPos.rangeClosed(new ChunkPos(minX >> 4, minZ >> 4), new ChunkPos(maxX >> 4, maxZ >> 4));
    }

    public boolean intersects(BoundingBox other) {
        return intersects(other.minX, other.minY, other.minZ, other.maxX, other.maxY, other.maxZ);
    }

    public boolean intersects(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        return this.maxX >= minX && this.minX <= maxX && this.maxZ >= minZ && this.minZ <= maxZ
            && this.maxY >= minY && this.minY <= maxY;
    }

    public boolean intersects(int minX, int minZ, int maxX, int maxZ) {
        return this.maxX >= minX && this.minX <= maxX && this.maxZ >= minZ && this.minZ <= maxZ;
    }

    public static Optional<BoundingBox> encapsulatingPositions(Iterable<BlockPos> positions) {
        BoundingBox result = null;
        for (BlockPos pos : positions) {
            result = result == null ? new BoundingBox(pos) : result.encapsulate(pos);
        }
        return Optional.ofNullable(result);
    }

    public static Optional<BoundingBox> encapsulatingBoxes(Iterable<BoundingBox> boxes) {
        BoundingBox result = null;
        for (BoundingBox box : boxes) {
            result = result == null ? new BoundingBox(box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ)
                : result.encapsulate(box);
        }
        return Optional.ofNullable(result);
    }

    public BoundingBox encapsulate(BoundingBox other) {
        minX = Math.min(minX, other.minX);
        minY = Math.min(minY, other.minY);
        minZ = Math.min(minZ, other.minZ);
        maxX = Math.max(maxX, other.maxX);
        maxY = Math.max(maxY, other.maxY);
        maxZ = Math.max(maxZ, other.maxZ);
        return this;
    }

    public static BoundingBox encapsulating(BoundingBox a, BoundingBox b) {
        return new BoundingBox(Math.min(a.minX, b.minX), Math.min(a.minY, b.minY), Math.min(a.minZ, b.minZ),
            Math.max(a.maxX, b.maxX), Math.max(a.maxY, b.maxY), Math.max(a.maxZ, b.maxZ));
    }

    public BoundingBox encapsulate(BlockPos pos) {
        minX = Math.min(minX, pos.getX());
        minY = Math.min(minY, pos.getY());
        minZ = Math.min(minZ, pos.getZ());
        maxX = Math.max(maxX, pos.getX());
        maxY = Math.max(maxY, pos.getY());
        maxZ = Math.max(maxZ, pos.getZ());
        return this;
    }

    public BoundingBox move(int dx, int dy, int dz) {
        minX += dx;
        minY += dy;
        minZ += dz;
        maxX += dx;
        maxY += dy;
        maxZ += dz;
        return this;
    }

    public BoundingBox move(Vec3i amount) {
        return move(amount.getX(), amount.getY(), amount.getZ());
    }

    public BoundingBox moved(int dx, int dy, int dz) {
        return new BoundingBox(minX + dx, minY + dy, minZ + dz, maxX + dx, maxY + dy, maxZ + dz);
    }

    public BoundingBox inflatedBy(int amount) {
        return inflatedBy(amount, amount, amount);
    }

    public BoundingBox inflatedBy(int x, int y, int z) {
        return new BoundingBox(minX - x, minY - y, minZ - z, maxX + x, maxY + y, maxZ + z);
    }

    public boolean isInside(Vec3i pos) {
        return isInside(pos.getX(), pos.getY(), pos.getZ());
    }

    public boolean isInside(int x, int y, int z) {
        return x >= minX && x <= maxX && z >= minZ && z <= maxZ && y >= minY && y <= maxY;
    }

    public Vec3i getLength() {
        return new Vec3i(maxX - minX, maxY - minY, maxZ - minZ);
    }

    public int getXSpan() {
        return maxX - minX + 1;
    }

    public int getYSpan() {
        return maxY - minY + 1;
    }

    public int getZSpan() {
        return maxZ - minZ + 1;
    }

    public BlockPos getCenter() {
        return new BlockPos(minX + (maxX - minX + 1) / 2, minY + (maxY - minY + 1) / 2, minZ + (maxZ - minZ + 1) / 2);
    }

    public void forAllCorners(Consumer<BlockPos> consumer) {
        for (int x : new int[] {minX, maxX}) {
            for (int y : new int[] {minY, maxY}) {
                for (int z : new int[] {minZ, maxZ}) {
                    consumer.accept(new BlockPos(x, y, z));
                }
            }
        }
    }

    public int minX() {
        return minX;
    }

    public int minY() {
        return minY;
    }

    public int minZ() {
        return minZ;
    }

    public int maxX() {
        return maxX;
    }

    public int maxY() {
        return maxY;
    }

    public int maxZ() {
        return maxZ;
    }

    @Override
    public boolean equals(Object o) {
        return o instanceof BoundingBox b && minX == b.minX && minY == b.minY && minZ == b.minZ && maxX == b.maxX
            && maxY == b.maxY && maxZ == b.maxZ;
    }

    @Override
    public int hashCode() {
        return java.util.Objects.hash(minX, minY, minZ, maxX, maxY, maxZ);
    }

    @Override
    public String toString() {
        return "BoundingBox[minX=" + minX + ", minY=" + minY + ", minZ=" + minZ + ", maxX=" + maxX + ", maxY=" + maxY
            + ", maxZ=" + maxZ + "]";
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

public class BlockPos extends Vec3i {
    public static final BlockPos ZERO = new BlockPos(0, 0, 0);
    private static final int kPackedXZ = 26;
    private static final int kPackedY = 12;

    public BlockPos(int x, int y, int z) {
        super(x, y, z);
    }

    public static long asLong(int x, int y, int z) {
        return ((long) x & 0x3FFFFFFL) << 38 | ((long) z & 0x3FFFFFFL) << 12 | (long) y & 0xFFFL;
    }

    public static BlockPos of(long packed) {
        return new BlockPos((int) (packed >> 38), (int) (packed << 52 >> 52), (int) (packed << 26 >> 38));
    }

    public long asLong() {
        return asLong(getX(), getY(), getZ());
    }

    public BlockPos offset(int dx, int dy, int dz) {
        return dx == 0 && dy == 0 && dz == 0 ? this : new BlockPos(getX() + dx, getY() + dy, getZ() + dz);
    }

    public BlockPos offset(Vec3i other) {
        return offset(other.getX(), other.getY(), other.getZ());
    }

    public BlockPos relative(Direction direction) {
        return offset(direction.getStepX(), direction.getStepY(), direction.getStepZ());
    }

    public BlockPos relative(Direction direction, int steps) {
        return offset(direction.getStepX() * steps, direction.getStepY() * steps, direction.getStepZ() * steps);
    }

    public BlockPos above() {
        return relative(Direction.UP);
    }

    public BlockPos below() {
        return relative(Direction.DOWN);
    }

    public BlockPos north() {
        return relative(Direction.NORTH);
    }

    public BlockPos south() {
        return relative(Direction.SOUTH);
    }

    public BlockPos west() {
        return relative(Direction.WEST);
    }

    public BlockPos east() {
        return relative(Direction.EAST);
    }

    public BlockPos immutable() {
        return this;
    }

    public static class MutableBlockPos extends BlockPos {
        public MutableBlockPos() {
            this(0, 0, 0);
        }

        public MutableBlockPos(int x, int y, int z) {
            super(x, y, z);
        }

        public MutableBlockPos set(int x, int y, int z) {
            setX(x);
            setY(y);
            setZ(z);
            return this;
        }

        public MutableBlockPos set(Vec3i pos) {
            return set(pos.getX(), pos.getY(), pos.getZ());
        }

        @Override
        public BlockPos immutable() {
            return new BlockPos(getX(), getY(), getZ());
        }
            public MutableBlockPos(double x, double y, double z) {
            super(Mth.floor(x), Mth.floor(y), Mth.floor(z));
        }
        public BlockPos offset(int x, int y, int z) {
            return new BlockPos(this.getX() + x, this.getY() + y, this.getZ() + z);
        }
        public BlockPos multiply(int scale) {
            return scale == 1 ? this : scale == 0 ? ZERO : new BlockPos(this.getX() * scale, this.getY() * scale, this.getZ() * scale);
        }
        public BlockPos relative(Direction direction, int steps) {
            return steps == 0 ? this : new BlockPos(this.getX() + direction.getStepX() * steps, this.getY() + direction.getStepY() * steps, this.getZ() + direction.getStepZ() * steps);
        }
        public BlockPos relative(Direction.Axis axis, int steps) {
            return steps == 0 ? this : new BlockPos(this.getX() + (axis == Direction.Axis.X ? steps : 0), this.getY() + (axis == Direction.Axis.Y ? steps : 0), this.getZ() + (axis == Direction.Axis.Z ? steps : 0));
        }
        public BlockPos rotate(Rotation rotation) {
            return super.rotate(rotation);
        }
        public MutableBlockPos set(double x, double y, double z) {
            return this.set(Mth.floor(x), Mth.floor(y), Mth.floor(z));
        }
        public MutableBlockPos set(long pos) {
            return this.set(getX(pos), getY(pos), getZ(pos));
        }
        public MutableBlockPos set(AxisCycle transform, int x, int y, int z) {
            return this.set(transform.cycle(x, y, z, Direction.Axis.X), transform.cycle(x, y, z, Direction.Axis.Y), transform.cycle(x, y, z, Direction.Axis.Z));
        }
        public MutableBlockPos setWithOffset(Vec3i pos, Direction direction) {
            return this.set(pos.getX() + direction.getStepX(), pos.getY() + direction.getStepY(), pos.getZ() + direction.getStepZ());
        }
        public MutableBlockPos setWithOffset(Vec3i pos, int x, int y, int z) {
            return this.set(pos.getX() + x, pos.getY() + y, pos.getZ() + z);
        }
        public MutableBlockPos setWithOffset(Vec3i pos, Vec3i offset) {
            return this.set(pos.getX() + offset.getX(), pos.getY() + offset.getY(), pos.getZ() + offset.getZ());
        }
        public MutableBlockPos move(Direction direction) {
            return this.move(direction, 1);
        }
        public MutableBlockPos move(Direction direction, int steps) {
            return this.set(this.getX() + direction.getStepX() * steps, this.getY() + direction.getStepY() * steps, this.getZ() + direction.getStepZ() * steps);
        }
        public MutableBlockPos move(int x, int y, int z) {
            return this.set(this.getX() + x, this.getY() + y, this.getZ() + z);
        }
        public MutableBlockPos move(Vec3i pos) {
            return this.set(this.getX() + pos.getX(), this.getY() + pos.getY(), this.getZ() + pos.getZ());
        }
        public MutableBlockPos clamp(Direction.Axis axis, int minimum, int maximum) {
            return switch (axis) {
            case X -> this.set(Mth.clamp(this.getX(), minimum, maximum), this.getY(), this.getZ());
            case Y -> this.set(this.getX(), Mth.clamp(this.getY(), minimum, maximum), this.getZ());
            case Z -> this.set(this.getX(), this.getY(), Mth.clamp(this.getZ(), minimum, maximum));
        };
        }
        public MutableBlockPos setX(int x) {
            super.setX(x);
        return this;
        }
        public MutableBlockPos setY(int y) {
            super.setY(y);
        return this;
        }
        public MutableBlockPos setZ(int z) {
            super.setZ(z);
        return this;
        }
}
    public static final Codec<BlockPos> CODEC = Codec.INT_STREAM.comapFlatMap(
        stream -> net.minecraft.util.Util.fixedSize(stream, 3).map(a -> new BlockPos(a[0], a[1], a[2])),
        pos -> java.util.stream.IntStream.of(pos.getX(), pos.getY(), pos.getZ())).stable();
    public static final StreamCodec<ByteBuf, BlockPos> STREAM_CODEC = StreamCodec.of(
        (buf, pos) -> buf.writeLong(pos.asLong()), buf -> of(buf.readLong()));
    public static final int PACKED_HORIZONTAL_LENGTH = 26;
    public static final int PACKED_Y_LENGTH = 12;
    public static final int MAX_HORIZONTAL_COORDINATE = 33554431;
    public static long offset(long blockNode, Direction offset) {
        return offset(blockNode, offset.getStepX(), offset.getStepY(), offset.getStepZ());
    }
    public static long offset(long blockNode, int stepX, int stepY, int stepZ) {
        return asLong(getX(blockNode) + stepX, getY(blockNode) + stepY, getZ(blockNode) + stepZ);
    }
    public static int getX(long blockNode) {
        return (int) (blockNode << 64 - 38 - 26 >> 64 - 26);
    }
    public static int getY(long blockNode) {
        return (int) (blockNode << 64 - 12 >> 64 - 12);
    }
    public static int getZ(long blockNode) {
        return (int) (blockNode << 64 - 12 - 26 >> 64 - 26);
    }
    public static BlockPos containing(double x, double y, double z) {
        return new BlockPos(Mth.floor(x), Mth.floor(y), Mth.floor(z));
    }
    public static BlockPos containing(Position pos) {
        return containing(pos.x(), pos.y(), pos.z());
    }
    public static BlockPos min(BlockPos a, BlockPos b) {
        return new BlockPos(Math.min(a.getX(), b.getX()), Math.min(a.getY(), b.getY()), Math.min(a.getZ(), b.getZ()));
    }
    public static BlockPos max(BlockPos a, BlockPos b) {
        return new BlockPos(Math.max(a.getX(), b.getX()), Math.max(a.getY(), b.getY()), Math.max(a.getZ(), b.getZ()));
    }
    public static long getFlatIndex(long neighborBlockNode) {
        return neighborBlockNode & -16L;
    }
    public BlockPos subtract(Vec3i vec) {
        return this.offset(-vec.getX(), -vec.getY(), -vec.getZ());
    }
    public BlockPos multiply(int scale) {
        return scale == 1 ? this : scale == 0 ? ZERO : new BlockPos(this.getX() * scale, this.getY() * scale, this.getZ() * scale);
    }
    public BlockPos above(int steps) {
        return this.relative(Direction.UP, steps);
    }
    public BlockPos below(int steps) {
        return this.relative(Direction.DOWN, steps);
    }
    public BlockPos north(int steps) {
        return this.relative(Direction.NORTH, steps);
    }
    public BlockPos south(int steps) {
        return this.relative(Direction.SOUTH, steps);
    }
    public BlockPos west(int steps) {
        return this.relative(Direction.WEST, steps);
    }
    public BlockPos east(int steps) {
        return this.relative(Direction.EAST, steps);
    }
    public BlockPos relative(Direction.Axis axis, int steps) {
        return steps == 0 ? this : new BlockPos(this.getX() + (axis == Direction.Axis.X ? steps : 0), this.getY() + (axis == Direction.Axis.Y ? steps : 0), this.getZ() + (axis == Direction.Axis.Z ? steps : 0));
    }
    public BlockPos rotate(Rotation rotation) {
        return switch (rotation) {
            case NONE -> this;
            case CLOCKWISE_90 -> new BlockPos(-this.getZ(), this.getY(), this.getX());
            case CLOCKWISE_180 -> new BlockPos(-this.getX(), this.getY(), -this.getZ());
            case COUNTERCLOCKWISE_90 -> new BlockPos(this.getZ(), this.getY(), -this.getX());
        };
    }
    public BlockPos cross(Vec3i upVector) {
        return new BlockPos(this.getY() * upVector.getZ() - this.getZ() * upVector.getY(), this.getZ() * upVector.getX() - this.getX() * upVector.getZ(), this.getX() * upVector.getY() - this.getY() * upVector.getX());
    }
    public BlockPos atY(int y) {
        return new BlockPos(this.getX(), y, this.getZ());
    }
    public MutableBlockPos mutable() {
        return new MutableBlockPos(this.getX(), this.getY(), this.getZ());
    }
    public Vec3 clampLocationWithin(Vec3 location) {
        return new Vec3(Mth.clamp(location.x, this.getX() + 1.0E-5F, this.getX() + 1.0 - 1.0E-5F), Mth.clamp(location.y, this.getY() + 1.0E-5F, this.getY() + 1.0 - 1.0E-5F), Mth.clamp(location.z, this.getZ() + 1.0E-5F, this.getZ() + 1.0 - 1.0E-5F));
    }
    public static Iterable<BlockPos> randomInCube(RandomSource random, int limit, BlockPos center, int sizeToScanInAllDirections) {
        return randomBetweenClosed(random, limit, center.getX() - sizeToScanInAllDirections, center.getY() - sizeToScanInAllDirections, center.getZ() - sizeToScanInAllDirections, center.getX() + sizeToScanInAllDirections, center.getY() + sizeToScanInAllDirections, center.getZ() + sizeToScanInAllDirections);
    }
    public static Iterable<BlockPos> randomBetweenClosed(RandomSource random, int limit, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.random(random, limit, minX, minY, minZ, maxX, maxY, maxZ);
    }
    public static Iterable<BlockPos> withinManhattan(BlockPos origin, int reach) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.manhattan(origin, reach, reach, reach, reach);
    }
    public static Iterable<BlockPos> withinClippedManhattan(BlockPos origin, int reachX, int reachY, int reachZ) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.manhattan(origin, reachX, reachY, reachZ, Math.max(Math.max(reachX, reachY), reachZ));
    }
    public static Iterable<BlockPos> withinBoxByManhattanDistance(BlockPos origin, int reachX, int reachY, int reachZ) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.manhattan(origin, reachX, reachY, reachZ, reachX + reachY + reachZ);
    }
    public static Iterable<BlockPos> betweenClosed(AABB box) {
        return betweenClosed(Mth.floor(box.minX), Mth.floor(box.minY), Mth.floor(box.minZ), Mth.floor(box.maxX), Mth.floor(box.maxY), Mth.floor(box.maxZ));
    }
    public static Iterable<BlockPos> betweenClosed(BlockPos a, BlockPos b) {
        return betweenClosed(Math.min(a.getX(), b.getX()), Math.min(a.getY(), b.getY()), Math.min(a.getZ(), b.getZ()), Math.max(a.getX(), b.getX()), Math.max(a.getY(), b.getY()), Math.max(a.getZ(), b.getZ()));
    }
    public static Stream<BlockPos> betweenClosedStream(BlockPos a, BlockPos b) {
        return betweenClosedStream(Math.min(a.getX(), b.getX()), Math.min(a.getY(), b.getY()), Math.min(a.getZ(), b.getZ()), Math.max(a.getX(), b.getX()), Math.max(a.getY(), b.getY()), Math.max(a.getZ(), b.getZ()));
    }
    public static Stream<BlockPos> betweenClosedStream(BoundingBox boundingBox) {
        return betweenClosedStream(Math.min(boundingBox.minX(), boundingBox.maxX()), Math.min(boundingBox.minY(), boundingBox.maxY()), Math.min(boundingBox.minZ(), boundingBox.maxZ()), Math.max(boundingBox.minX(), boundingBox.maxX()), Math.max(boundingBox.minY(), boundingBox.maxY()), Math.max(boundingBox.minZ(), boundingBox.maxZ()));
    }
    public static Stream<BlockPos> betweenClosedStream(AABB box) {
        return betweenClosedStream(Mth.floor(box.minX), Mth.floor(box.minY), Mth.floor(box.minZ), Mth.floor(box.maxX), Mth.floor(box.maxY), Mth.floor(box.maxZ));
    }
    public static Stream<BlockPos> betweenClosedStream(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        return java.util.stream.StreamSupport.stream(betweenClosed(minX, minY, minZ, maxX, maxY, maxZ).spliterator(), false);
    }
    public static Iterable<BlockPos> betweenClosed(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        int width = maxX - minX + 1;
        int height = maxY - minY + 1;
        int depth = maxZ - minZ + 1;
        int total = width * height * depth;
        return () -> new java.util.Iterator<BlockPos>() {
            private final MutableBlockPos cursor = new MutableBlockPos();
            private int index;

            @Override
            public boolean hasNext() {
                return this.index < total;
            }

            @Override
            public BlockPos next() {
                if (this.index >= total) {
                    throw new java.util.NoSuchElementException();
                }
                int x = this.index % width;
                int slice = this.index / width;
                this.index++;
                return this.cursor.set(minX + x, minY + slice % height, minZ + slice / height);
            }
        };
    }
    public static Iterable<BlockPos> neighborColumn(int startX, int startY, int startZ, int endY) {
        int dir = endY > startY ? 1 : -1;
        int height = Math.abs(endY - startY) + 1;
        Vec3i[] steps = {new Vec3i(0, 0, 0), Direction.NORTH.getUnitVec3i(), Direction.EAST.getUnitVec3i(), Direction.SOUTH.getUnitVec3i(), Direction.WEST.getUnitVec3i()};
        int total = steps.length * height;
        return () -> new java.util.Iterator<BlockPos>() {
            private final MutableBlockPos cursor = new MutableBlockPos();
            private int index;

            @Override
            public boolean hasNext() {
                return this.index < total;
            }

            @Override
            public BlockPos next() {
                if (this.index >= total) {
                    throw new java.util.NoSuchElementException();
                }
                Vec3i step = steps[this.index / height];
                int y = this.index % height;
                this.index++;
                return this.cursor.set(startX + step.getX(), startY + y * dir, startZ + step.getZ());
            }
        };
    }
    public static Iterable<MutableBlockPos> spiralAround(BlockPos center, int radius, Direction firstDirection, Direction secondDirection) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.spiral(center, radius, firstDirection, secondDirection);
    }
    public static int breadthFirstTraversal(BlockPos startPos, int maxDepth, int maxCount, BiConsumer<BlockPos, Consumer<BlockPos>> neighbourProvider, Function<BlockPos, TraversalNodeStatus> nodeProcessor) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.breadthFirst(startPos, maxDepth, maxCount, neighbourProvider, nodeProcessor);
    }
    public static Iterable<BlockPos> betweenCornersInDirection(AABB aabb, Vec3 direction) {
        Vec3 min = aabb.getMinPosition();
        Vec3 max = aabb.getMaxPosition();
        return betweenCornersInDirection(Mth.floor(min.x()), Mth.floor(min.y()), Mth.floor(min.z()), Mth.floor(max.x()), Mth.floor(max.y()), Mth.floor(max.z()), direction);
    }
    public static Iterable<BlockPos> betweenCornersInDirection(BlockPos firstCorner, BlockPos secondCorner, Vec3 direction) {
        return betweenCornersInDirection(firstCorner.getX(), firstCorner.getY(), firstCorner.getZ(), secondCorner.getX(), secondCorner.getY(), secondCorner.getZ(), direction);
    }
    public static Iterable<BlockPos> betweenCornersInDirection(int firstCornerX, int firstCornerY, int firstCornerZ, int secondCornerX, int secondCornerY, int secondCornerZ, Vec3 direction) {
        return org.basaltmc.bukkit.facade.BlockPosIterators.cornersInDirection(firstCornerX, firstCornerY, firstCornerZ, secondCornerX, secondCornerY, secondCornerZ, direction);
    }
    public static enum TraversalNodeStatus {
        ACCEPT, SKIP, STOP;
    }
}

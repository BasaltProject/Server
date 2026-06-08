package net.minecraft.core;

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
    }
}

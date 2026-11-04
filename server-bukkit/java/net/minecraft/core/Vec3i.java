// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.Mth;
import org.joml.Vector3i;

public class Vec3i implements Comparable<Vec3i> {
    public static final Vec3i ZERO = new Vec3i(0, 0, 0);
    private int x;
    private int y;
    private int z;

    public Vec3i(int x, int y, int z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public int getX() {
        return x;
    }

    public int getY() {
        return y;
    }

    public int getZ() {
        return z;
    }

    protected Vec3i setX(int x) {
        this.x = x;
        return this;
    }

    protected Vec3i setY(int y) {
        this.y = y;
        return this;
    }

    protected Vec3i setZ(int z) {
        this.z = z;
        return this;
    }

    @Override
    public boolean equals(Object other) {
        return this == other || other instanceof Vec3i v && x == v.x && y == v.y && z == v.z;
    }

    @Override
    public int hashCode() {
        return (y + z * 31) * 31 + x;
    }

    @Override
    public int compareTo(Vec3i other) {
        return y != other.y ? y - other.y : z != other.z ? z - other.z : x - other.x;
    }

    @Override
    public String toString() {
        return getClass().getSimpleName() + "{x=" + x + ", y=" + y + ", z=" + z + "}";
    }
    public static final Codec<Vec3i> CODEC = Codec.INT_STREAM.comapFlatMap(
        stream -> net.minecraft.util.Util.fixedSize(stream, 3).map(a -> new Vec3i(a[0], a[1], a[2])),
        vec -> java.util.stream.IntStream.of(vec.getX(), vec.getY(), vec.getZ())).stable();
    public static final StreamCodec<ByteBuf, Vec3i> STREAM_CODEC = StreamCodec.composite(net.minecraft.network.codec.ByteBufCodecs.VAR_INT, Vec3i::getX,
        net.minecraft.network.codec.ByteBufCodecs.VAR_INT, Vec3i::getY, net.minecraft.network.codec.ByteBufCodecs.VAR_INT, Vec3i::getZ, Vec3i::new);
    public static Codec<Vec3i> offsetCodec(int maxOffsetPerAxis) {
        return CODEC.validate(vec -> Math.abs(vec.getX()) < maxOffsetPerAxis && Math.abs(vec.getY()) < maxOffsetPerAxis
            && Math.abs(vec.getZ()) < maxOffsetPerAxis ? com.mojang.serialization.DataResult.success(vec)
            : com.mojang.serialization.DataResult.error(() -> "Position out of range, expected at most " + maxOffsetPerAxis + ": " + vec));
    }
    public Vec3i offset(int x, int y, int z) {
        return x == 0 && y == 0 && z == 0 ? this : new Vec3i(this.getX() + x, this.getY() + y, this.getZ() + z);
    }
    public Vec3i offset(Vec3i vec) {
        return this.offset(vec.getX(), vec.getY(), vec.getZ());
    }
    public Vec3i subtract(Vec3i vec) {
        return this.offset(-vec.getX(), -vec.getY(), -vec.getZ());
    }
    public Vec3i multiply(int scale) {
        return scale == 1 ? this : scale == 0 ? ZERO : new Vec3i(this.getX() * scale, this.getY() * scale, this.getZ() * scale);
    }
    public Vec3i multiply(int xScale, int yScale, int zScale) {
        return new Vec3i(this.getX() * xScale, this.getY() * yScale, this.getZ() * zScale);
    }
    public Vec3i above() {
        return this.above(1);
    }
    public Vec3i above(int steps) {
        return this.relative(Direction.UP, steps);
    }
    public Vec3i below() {
        return this.below(1);
    }
    public Vec3i below(int steps) {
        return this.relative(Direction.DOWN, steps);
    }
    public Vec3i north() {
        return this.north(1);
    }
    public Vec3i north(int steps) {
        return this.relative(Direction.NORTH, steps);
    }
    public Vec3i south() {
        return this.south(1);
    }
    public Vec3i south(int steps) {
        return this.relative(Direction.SOUTH, steps);
    }
    public Vec3i west() {
        return this.west(1);
    }
    public Vec3i west(int steps) {
        return this.relative(Direction.WEST, steps);
    }
    public Vec3i east() {
        return this.east(1);
    }
    public Vec3i east(int steps) {
        return this.relative(Direction.EAST, steps);
    }
    public Vec3i relative(Direction direction) {
        return this.relative(direction, 1);
    }
    public Vec3i relative(Direction direction, int steps) {
        return steps == 0 ? this : new Vec3i(this.getX() + direction.getStepX() * steps, this.getY() + direction.getStepY() * steps, this.getZ() + direction.getStepZ() * steps);
    }
    public Vec3i relative(Direction.Axis axis, int steps) {
        if (steps == 0) {
            return this;
        }
        int dx = axis == Direction.Axis.X ? steps : 0;
        int dy = axis == Direction.Axis.Y ? steps : 0;
        int dz = axis == Direction.Axis.Z ? steps : 0;
        return new Vec3i(this.getX() + dx, this.getY() + dy, this.getZ() + dz);
    }
    public Vec3i cross(Vec3i upVector) {
        return new Vec3i(this.getY() * upVector.getZ() - this.getZ() * upVector.getY(), this.getZ() * upVector.getX() - this.getX() * upVector.getZ(), this.getX() * upVector.getY() - this.getY() * upVector.getX());
    }
    public boolean closerThan(Vec3i pos, double distance) {
        return this.distSqr(pos) < Mth.square(distance);
    }
    public boolean closerToCenterThan(Position pos, double distance) {
        return this.distToCenterSqr(pos) < Mth.square(distance);
    }
    public double distSqr(Vec3i pos) {
        return this.distToLowCornerSqr(pos.getX(), pos.getY(), pos.getZ());
    }
    public double distToCenterSqr(Position pos) {
        return this.distToCenterSqr(pos.x(), pos.y(), pos.z());
    }
    public double distToCenterSqr(double x, double y, double z) {
        double dx = this.getX() + 0.5 - x;
        double dy = this.getY() + 0.5 - y;
        double dz = this.getZ() + 0.5 - z;
        return dx * dx + dy * dy + dz * dz;
    }
    public double distToLowCornerSqr(double x, double y, double z) {
        double dx = this.getX() - x;
        double dy = this.getY() - y;
        double dz = this.getZ() - z;
        return dx * dx + dy * dy + dz * dz;
    }
    public int distManhattan(Vec3i pos) {
        return Math.abs(pos.getX() - this.getX()) + Math.abs(pos.getY() - this.getY()) + Math.abs(pos.getZ() - this.getZ());
    }
    public int distChessboard(Vec3i pos) {
        return Math.max(Math.max(Math.abs(pos.getX() - this.getX()), Math.abs(pos.getY() - this.getY())), Math.abs(pos.getZ() - this.getZ()));
    }
    public boolean differsHorizontally(Vec3i pos) {
        return this.getX() != pos.getX() || this.getZ() != pos.getZ();
    }
    public int get(Direction.Axis axis) {
        return axis.choose(this.getX(), this.getY(), this.getZ());
    }
    public Vector3i toMutable() {
        return new Vector3i(this.getX(), this.getY(), this.getZ());
    }
    public String toShortString() {
        return this.getX() + ", " + this.getY() + ", " + this.getZ();
    }
}

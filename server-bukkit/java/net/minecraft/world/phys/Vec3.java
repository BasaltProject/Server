// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys;

import net.minecraft.core.Position;
import net.minecraft.core.Vec3i;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.EnumSet;
import net.minecraft.core.Direction;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class Vec3 implements Position {
    public static final Vec3 ZERO = new Vec3(0.0, 0.0, 0.0);
    public final double x;
    public final double y;
    public final double z;

    public Vec3(double x, double y, double z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public static Vec3 atLowerCornerOf(Vec3i pos) {
        return new Vec3(pos.getX(), pos.getY(), pos.getZ());
    }

    public static Vec3 atCenterOf(Vec3i pos) {
        return new Vec3(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5);
    }

    @Override
    public double x() {
        return x;
    }

    @Override
    public double y() {
        return y;
    }

    @Override
    public double z() {
        return z;
    }
    public static final Codec<Vec3> CODEC = Codec.DOUBLE.listOf().comapFlatMap(
        list -> net.minecraft.util.Util.fixedSize(list, 3).map(d -> new Vec3(d.get(0), d.get(1), d.get(2))),
        pos -> java.util.List.of(pos.x(), pos.y(), pos.z()));
    public static final StreamCodec<ByteBuf, Vec3> STREAM_CODEC = StreamCodec.of(
        (buf, v) -> {
            buf.writeDouble(v.x());
            buf.writeDouble(v.y());
            buf.writeDouble(v.z());
        }, buf -> new Vec3(buf.readDouble(), buf.readDouble(), buf.readDouble()));
    public static final StreamCodec<ByteBuf, Vec3> LP_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.phys.Vec3.class, "LP_STREAM_CODEC");
    public static final Vec3 X_AXIS = new Vec3(1.0, 0.0, 0.0);
    public static final Vec3 Y_AXIS = new Vec3(0.0, 1.0, 0.0);
    public static final Vec3 Z_AXIS = new Vec3(0.0, 0.0, 1.0);
    public static Vec3 atLowerCornerWithOffset(Vec3i pos, double x, double y, double z) {
        return new Vec3(pos.getX() + x, pos.getY() + y, pos.getZ() + z);
    }
    public static Vec3 atCenterOfWithY(Vec3i pos, double y) {
        return new Vec3(pos.getX() + 0.5, y, pos.getZ() + 0.5);
    }
    public static Vec3 atBottomCenterOf(Vec3i pos) {
        return atLowerCornerWithOffset(pos, 0.5, 0.0, 0.5);
    }
    public static Vec3 upFromBottomCenterOf(Vec3i pos, double yOffset) {
        return atLowerCornerWithOffset(pos, 0.5, yOffset, 0.5);
    }
    public Vec3(Vector3fc vec) {
        this(vec.x(), vec.y(), vec.z());
    }
    public Vec3(Vec3i vec) {
        this(vec.getX(), vec.getY(), vec.getZ());
    }
    public Vec3 vectorTo(Vec3 vec) {
        return new Vec3(vec.x - this.x, vec.y - this.y, vec.z - this.z);
    }
    public Vec3 normalize() {
        double len = Math.sqrt(this.x * this.x + this.y * this.y + this.z * this.z);
        return len < 1.0E-5 ? ZERO : new Vec3(this.x / len, this.y / len, this.z / len);
    }
    public double dot(Vec3 vec) {
        return this.x * vec.x + this.y * vec.y + this.z * vec.z;
    }
    public Vec3 cross(Vec3 vec) {
        return new Vec3(this.y * vec.z - this.z * vec.y, this.z * vec.x - this.x * vec.z, this.x * vec.y - this.y * vec.x);
    }
    public Vec3 subtract(Vec3 vec) {
        return this.subtract(vec.x, vec.y, vec.z);
    }
    public Vec3 subtract(double value) {
        return this.subtract(value, value, value);
    }
    public Vec3 subtract(double x, double y, double z) {
        return this.add(-x, -y, -z);
    }
    public Vec3 add(double value) {
        return this.add(value, value, value);
    }
    public Vec3 add(Vec3 vec) {
        return this.add(vec.x, vec.y, vec.z);
    }
    public Vec3 add(double x, double y, double z) {
        return new Vec3(this.x + x, this.y + y, this.z + z);
    }
    public boolean closerThan(Position pos, double distance) {
        return this.distanceToSqr(pos.x(), pos.y(), pos.z()) < distance * distance;
    }
    public double distanceTo(Vec3 vec) {
        double dx = vec.x - this.x;
        double dy = vec.y - this.y;
        double dz = vec.z - this.z;
        return Math.sqrt(dx * dx + dy * dy + dz * dz);
    }
    public double distanceToSqr(Vec3 vec) {
        return this.distanceToSqr(vec.x, vec.y, vec.z);
    }
    public double distanceToSqr(double x, double y, double z) {
        double dx = x - this.x;
        double dy = y - this.y;
        double dz = z - this.z;
        return dx * dx + dy * dy + dz * dz;
    }
    public boolean closerThan(Vec3 vec, double distanceXZ, double distanceY) {
        double dx = vec.x() - this.x;
        double dy = vec.y() - this.y;
        double dz = vec.z() - this.z;
        return Mth.lengthSquared(dx, dz) < Mth.square(distanceXZ) && Math.abs(dy) < distanceY;
    }
    public Vec3 scale(double scale) {
        return this.multiply(scale, scale, scale);
    }
    public Vec3 reverse() {
        return this.scale(-1.0);
    }
    public Vec3 multiply(Vec3 scale) {
        return this.multiply(scale.x, scale.y, scale.z);
    }
    public Vec3 multiply(double xScale, double yScale, double zScale) {
        return new Vec3(this.x * xScale, this.y * yScale, this.z * zScale);
    }
    public Vec3 horizontal() {
        return new Vec3(this.x, 0.0, this.z);
    }
    public Vec3 offsetRandom(RandomSource random, float offset) {
        return this.add((random.nextFloat() - 0.5F) * offset, (random.nextFloat() - 0.5F) * offset, (random.nextFloat() - 0.5F) * offset);
    }
    public Vec3 offsetRandomXZ(RandomSource random, float offset) {
        return this.add((random.nextFloat() - 0.5F) * offset, 0.0, (random.nextFloat() - 0.5F) * offset);
    }
    public double length() {
        return Math.sqrt(this.lengthSqr());
    }
    public double lengthSqr() {
        return this.x * this.x + this.y * this.y + this.z * this.z;
    }
    public double horizontalDistance() {
        return Math.sqrt(this.horizontalDistanceSqr());
    }
    public double horizontalDistanceSqr() {
        return this.x * this.x + this.z * this.z;
    }
    public boolean equals(Object o) {
        return this == o || o instanceof Vec3 other && Double.compare(other.x, this.x) == 0 && Double.compare(other.y, this.y) == 0 && Double.compare(other.z, this.z) == 0;
    }
    public int hashCode() {
        int result = Long.hashCode(Double.doubleToLongBits(this.x));
        result = 31 * result + Long.hashCode(Double.doubleToLongBits(this.y));
        return 31 * result + Long.hashCode(Double.doubleToLongBits(this.z));
    }
    public String toString() {
        return "(" + this.x + ", " + this.y + ", " + this.z + ")";
    }
    public Vec3 lerp(Vec3 vec, double a) {
        return new Vec3(Mth.lerp(a, this.x, vec.x), Mth.lerp(a, this.y, vec.y), Mth.lerp(a, this.z, vec.z));
    }
    public Vec3 xRot(float radians) {
        float cos = Mth.cos(radians);
        float sin = Mth.sin(radians);
        return new Vec3(this.x, this.y * cos + this.z * sin, this.z * cos - this.y * sin);
    }
    public Vec3 yRot(float radians) {
        float cos = Mth.cos(radians);
        float sin = Mth.sin(radians);
        return new Vec3(this.x * cos + this.z * sin, this.y, this.z * cos - this.x * sin);
    }
    public Vec3 zRot(float radians) {
        float cos = Mth.cos(radians);
        float sin = Mth.sin(radians);
        return new Vec3(this.x * cos + this.y * sin, this.y * cos - this.x * sin, this.z);
    }
    public Vec3 rotateClockwise90() {
        return new Vec3(-this.z, this.y, this.x);
    }
    public static Vec3 directionFromRotation(Vec2 rotation) {
        return directionFromRotation(rotation.x, rotation.y);
    }
    public static Vec3 directionFromRotation(float rotX, float rotY) {
        float yCos = Mth.cos(-rotY * Mth.DEG_TO_RAD - Mth.PI);
        float ySin = Mth.sin(-rotY * Mth.DEG_TO_RAD - Mth.PI);
        float xCos = -Mth.cos(-rotX * Mth.DEG_TO_RAD);
        float xSin = Mth.sin(-rotX * Mth.DEG_TO_RAD);
        return new Vec3(ySin * xCos, xSin, yCos * xCos);
    }
    public Vec2 rotation() {
        float yaw = (float) Math.atan2(-this.x, this.z) * Mth.RAD_TO_DEG;
        float pitch = (float) Math.asin(-this.y / Math.sqrt(this.lengthSqr())) * Mth.RAD_TO_DEG;
        return new Vec2(pitch, yaw);
    }
    public Vec3 align(EnumSet<Direction.Axis> axes) {
        return new Vec3(axes.contains(Direction.Axis.X) ? Mth.floor(this.x) : this.x, axes.contains(Direction.Axis.Y) ? Mth.floor(this.y) : this.y, axes.contains(Direction.Axis.Z) ? Mth.floor(this.z) : this.z);
    }
    public double get(Direction.Axis axis) {
        return axis.choose(this.x, this.y, this.z);
    }
    public Vec3 with(Direction.Axis axis, double value) {
        return new Vec3(axis == Direction.Axis.X ? value : this.x, axis == Direction.Axis.Y ? value : this.y, axis == Direction.Axis.Z ? value : this.z);
    }
    public Vec3 relative(Direction direction, double distance) {
        Vec3i step = direction.getUnitVec3i();
        return new Vec3(this.x + distance * step.getX(), this.y + distance * step.getY(), this.z + distance * step.getZ());
    }
    public Vector3f toVector3f() {
        return new Vector3f((float) this.x, (float) this.y, (float) this.z);
    }
    public Vec3 projectedOn(Vec3 onto) {
        return onto.lengthSqr() == 0.0 ? onto : onto.scale(this.dot(onto)).scale(1.0 / onto.lengthSqr());
    }
    public boolean isFinite() {
        return Double.isFinite(this.x) && Double.isFinite(this.y) && Double.isFinite(this.z);
    }
}

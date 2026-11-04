// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.Iterators;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import io.netty.buffer.ByteBuf;
import java.util.Arrays;
import java.util.Collection;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.function.IntFunction;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ByIdMap;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.Util;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public enum Direction implements Directional, StringRepresentable {
    DOWN(0, 1, -1, "down", AxisDirection.NEGATIVE, Axis.Y, new Vec3i(0, -1, 0)),
    UP(1, 0, -1, "up", AxisDirection.POSITIVE, Axis.Y, new Vec3i(0, 1, 0)),
    NORTH(2, 3, 2, "north", AxisDirection.NEGATIVE, Axis.Z, new Vec3i(0, 0, -1)),
    SOUTH(3, 2, 0, "south", AxisDirection.POSITIVE, Axis.Z, new Vec3i(0, 0, 1)),
    WEST(4, 5, 1, "west", AxisDirection.NEGATIVE, Axis.X, new Vec3i(-1, 0, 0)),
    EAST(5, 4, 3, "east", AxisDirection.POSITIVE, Axis.X, new Vec3i(1, 0, 0));

    public static final StringRepresentable.EnumCodec<Direction> CODEC = StringRepresentable.fromEnum(Direction::values);
    public static final Codec<Direction> VERTICAL_CODEC = CODEC.validate(Direction::verifyVertical);
    public static final IntFunction<Direction> BY_ID = ByIdMap.continuous(Direction::get3DDataValue, values(),
        ByIdMap.OutOfBoundsStrategy.WRAP);
    public static final StreamCodec<ByteBuf, Direction> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID,
        Direction::get3DDataValue);
    @Deprecated
    public static final Codec<Direction> LEGACY_ID_CODEC = Codec.BYTE.xmap(Direction::from3DDataValue,
        d -> (byte) d.get3DDataValue());
    @Deprecated
    public static final Codec<Direction> LEGACY_ID_CODEC_2D = Codec.BYTE.xmap(Direction::from2DDataValue,
        d -> (byte) d.get2DDataValue());

    private static final ImmutableList<Axis> kYxzOrder = ImmutableList.of(Axis.Y, Axis.X, Axis.Z);
    private static final ImmutableList<Axis> kYzxOrder = ImmutableList.of(Axis.Y, Axis.Z, Axis.X);
    private static final Direction[] kAll = values();
    private static final Direction[] kBy3d = Arrays.stream(kAll).sorted(Comparator.comparingInt(d -> d.data3d))
        .toArray(Direction[]::new);
    private static final Direction[] kBy2d = Arrays.stream(kAll).filter(d -> d.getAxis().isHorizontal())
        .sorted(Comparator.comparingInt(d -> d.data2d)).toArray(Direction[]::new);

    private final int data3d;
    private final int oppositeIndex;
    private final int data2d;
    private final String name;
    private final Axis axis;
    private final AxisDirection axisDirection;
    private final Vec3i normal;
    private final Vec3 normalVec3;
    private final Vector3fc normalVec3f;

    Direction(int data3d, int oppositeIndex, int data2d, String name, AxisDirection axisDirection, Axis axis,
        Vec3i normal) {
        this.data3d = data3d;
        this.oppositeIndex = oppositeIndex;
        this.data2d = data2d;
        this.name = name;
        this.axis = axis;
        this.axisDirection = axisDirection;
        this.normal = normal;
        this.normalVec3 = Vec3.atLowerCornerOf(normal);
        this.normalVec3f = new Vector3f(normal.getX(), normal.getY(), normal.getZ());
    }

    public static Direction[] orderedByNearest(Entity entity) {
        float pitch = entity.getViewXRot(1.0F) * Mth.DEG_TO_RAD;
        float yaw = -entity.getViewYRot(1.0F) * Mth.DEG_TO_RAD;
        float pitchSin = Mth.sin(pitch);
        float pitchCos = Mth.cos(pitch);
        float yawSin = Mth.sin(yaw);
        float yawCos = Mth.cos(yaw);
        boolean xPositive = yawSin > 0.0F;
        boolean yPositive = pitchSin < 0.0F;
        boolean zPositive = yawCos > 0.0F;
        float xYaw = xPositive ? yawSin : -yawSin;
        float yMag = yPositive ? -pitchSin : pitchSin;
        float zYaw = zPositive ? yawCos : -yawCos;
        float xMag = xYaw * pitchCos;
        float zMag = zYaw * pitchCos;
        Direction dx = xPositive ? EAST : WEST;
        Direction dy = yPositive ? UP : DOWN;
        Direction dz = zPositive ? SOUTH : NORTH;
        if (xYaw > zYaw) {
            if (yMag > xMag) {
                return withOpposites(dy, dx, dz);
            }
            return zMag > yMag ? withOpposites(dx, dz, dy) : withOpposites(dx, dy, dz);
        }
        if (yMag > zMag) {
            return withOpposites(dy, dz, dx);
        }
        return xMag > yMag ? withOpposites(dz, dx, dy) : withOpposites(dz, dy, dx);
    }

    private static Direction[] withOpposites(Direction first, Direction second, Direction third) {
        return new Direction[] {first, second, third, third.getOpposite(), second.getOpposite(), first.getOpposite()};
    }

    public static Direction rotate(Matrix4fc matrix, Direction facing) {
        Vector3f vec = matrix.transformDirection(facing.normalVec3f, new Vector3f());
        return getApproximateNearest(vec.x(), vec.y(), vec.z());
    }

    public static Collection<Direction> allShuffled(RandomSource random) {
        return Util.shuffledCopy(values(), random);
    }

    public static Stream<Direction> stream() {
        return Stream.of(kAll);
    }

    public static float getYRot(Direction direction) {
        return switch (direction) {
            case NORTH -> 180.0F;
            case SOUTH -> 0.0F;
            case WEST -> 90.0F;
            case EAST -> -90.0F;
            default -> throw new IllegalStateException("No y-Rot for vertical axis: " + direction);
        };
    }

    public Quaternionf getRotation() {
        return switch (this) {
            case DOWN -> new Quaternionf().rotationX(Mth.PI);
            case UP -> new Quaternionf();
            case NORTH -> new Quaternionf().rotationXYZ(Mth.HALF_PI, 0.0F, Mth.PI);
            case SOUTH -> new Quaternionf().rotationX(Mth.HALF_PI);
            case WEST -> new Quaternionf().rotationXYZ(Mth.HALF_PI, 0.0F, Mth.HALF_PI);
            case EAST -> new Quaternionf().rotationXYZ(Mth.HALF_PI, 0.0F, -Mth.HALF_PI);
        };
    }

    public int get3DDataValue() {
        return this.data3d;
    }

    public int get2DDataValue() {
        return this.data2d;
    }

    public AxisDirection getAxisDirection() {
        return this.axisDirection;
    }

    public static Direction getFacingAxis(Entity entity, Axis axis) {
        return switch (axis) {
            case X -> EAST.isFacingAngle(entity.getViewYRot(1.0F)) ? EAST : WEST;
            case Y -> entity.getViewXRot(1.0F) < 0.0F ? UP : DOWN;
            case Z -> SOUTH.isFacingAngle(entity.getViewYRot(1.0F)) ? SOUTH : NORTH;
        };
    }

    public Direction getOpposite() {
        return from3DDataValue(this.oppositeIndex);
    }

    public Direction getClockWise(Axis axis) {
        return switch (axis) {
            case X -> this != WEST && this != EAST ? this.getClockWiseX() : this;
            case Y -> this != UP && this != DOWN ? this.getClockWise() : this;
            case Z -> this != NORTH && this != SOUTH ? this.getClockWiseZ() : this;
        };
    }

    public Direction getCounterClockWise(Axis axis) {
        return switch (axis) {
            case X -> this != WEST && this != EAST ? this.getCounterClockWiseX() : this;
            case Y -> this != UP && this != DOWN ? this.getCounterClockWise() : this;
            case Z -> this != NORTH && this != SOUTH ? this.getCounterClockWiseZ() : this;
        };
    }

    public Direction getClockWise() {
        return switch (this) {
            case NORTH -> EAST;
            case SOUTH -> WEST;
            case WEST -> NORTH;
            case EAST -> SOUTH;
            default -> throw new IllegalStateException("Unable to get Y-rotated facing of " + this);
        };
    }

    private Direction getClockWiseX() {
        return switch (this) {
            case DOWN -> SOUTH;
            case UP -> NORTH;
            case NORTH -> DOWN;
            case SOUTH -> UP;
            default -> throw new IllegalStateException("Unable to get X-rotated facing of " + this);
        };
    }

    private Direction getCounterClockWiseX() {
        return switch (this) {
            case DOWN -> NORTH;
            case UP -> SOUTH;
            case NORTH -> UP;
            case SOUTH -> DOWN;
            default -> throw new IllegalStateException("Unable to get X-rotated facing of " + this);
        };
    }

    private Direction getClockWiseZ() {
        return switch (this) {
            case DOWN -> WEST;
            case UP -> EAST;
            case WEST -> UP;
            case EAST -> DOWN;
            default -> throw new IllegalStateException("Unable to get Z-rotated facing of " + this);
        };
    }

    private Direction getCounterClockWiseZ() {
        return switch (this) {
            case DOWN -> EAST;
            case UP -> WEST;
            case WEST -> DOWN;
            case EAST -> UP;
            default -> throw new IllegalStateException("Unable to get Z-rotated facing of " + this);
        };
    }

    public Direction getCounterClockWise() {
        return switch (this) {
            case NORTH -> WEST;
            case SOUTH -> EAST;
            case WEST -> SOUTH;
            case EAST -> NORTH;
            default -> throw new IllegalStateException("Unable to get CCW facing of " + this);
        };
    }

    public int getStepX() {
        return this.normal.getX();
    }

    public int getStepY() {
        return this.normal.getY();
    }

    public int getStepZ() {
        return this.normal.getZ();
    }

    public Vec3i getStep() {
        return this.normal;
    }

    public Vector3f step() {
        return new Vector3f(this.normalVec3f);
    }

    public String getName() {
        return this.name;
    }

    public Axis getAxis() {
        return this.axis;
    }

    public static Direction byName(String name) {
        return CODEC.byName(name);
    }

    public static Direction from3DDataValue(int data) {
        return kBy3d[Mth.abs(data % kBy3d.length)];
    }

    public static Direction from2DDataValue(int data) {
        return kBy2d[Mth.abs(data % kBy2d.length)];
    }

    public static Direction fromYRot(double yRot) {
        return from2DDataValue(Mth.floor(yRot / 90.0 + 0.5) & 3);
    }

    public static Direction fromAxisAndDirection(Axis axis, AxisDirection direction) {
        boolean positive = direction == AxisDirection.POSITIVE;
        return switch (axis) {
            case X -> positive ? EAST : WEST;
            case Y -> positive ? UP : DOWN;
            case Z -> positive ? SOUTH : NORTH;
        };
    }

    public float toYRot() {
        return (this.data2d & 3) * 90;
    }

    public static Direction getRandom(RandomSource random) {
        return Util.getRandom(kAll, random);
    }

    public static Direction getApproximateNearest(double dx, double dy, double dz) {
        return getApproximateNearest((float) dx, (float) dy, (float) dz);
    }

    public static Direction getApproximateNearest(float dx, float dy, float dz) {
        Direction best = NORTH;
        float bestDot = Float.MIN_VALUE;
        for (Direction direction : kAll) {
            float dot = dx * direction.normal.getX() + dy * direction.normal.getY() + dz * direction.normal.getZ();
            if (dot > bestDot) {
                bestDot = dot;
                best = direction;
            }
        }
        return best;
    }

    public static Direction getApproximateNearest(Vec3 vec) {
        return getApproximateNearest(vec.x, vec.y, vec.z);
    }

    public static Direction getNearest(int x, int y, int z, Direction orElse) {
        int ax = Math.abs(x);
        int ay = Math.abs(y);
        int az = Math.abs(z);
        if (ax > az && ax > ay) {
            return x < 0 ? WEST : EAST;
        }
        if (az > ax && az > ay) {
            return z < 0 ? NORTH : SOUTH;
        }
        if (ay > ax && ay > az) {
            return y < 0 ? DOWN : UP;
        }
        return orElse;
    }

    public static Direction getNearest(Vec3i vec, Direction orElse) {
        return getNearest(vec.getX(), vec.getY(), vec.getZ(), orElse);
    }

    @Override
    public String toString() {
        return this.name;
    }

    @Override
    public String getSerializedName() {
        return this.name;
    }

    private static DataResult<Direction> verifyVertical(Direction value) {
        return value.getAxis().isVertical() ? DataResult.success(value) : DataResult.error(() -> "Expected a vertical direction");
    }

    public static Direction get(AxisDirection axisDirection, Axis axis) {
        for (Direction direction : kAll) {
            if (direction.axisDirection == axisDirection && direction.axis == axis) {
                return direction;
            }
        }
        throw new IllegalArgumentException("No such direction: " + axisDirection + " " + axis);
    }

    public static ImmutableList<Axis> axisStepOrder(Vec3 movement) {
        return Math.abs(movement.x) < Math.abs(movement.z) ? kYzxOrder : kYxzOrder;
    }

    public Vec3i getUnitVec3i() {
        return this.normal;
    }

    public Vec3 getUnitVec3() {
        return this.normalVec3;
    }

    public Vector3fc getUnitVec3f() {
        return this.normalVec3f;
    }

    public boolean isFacingAngle(float yAngle) {
        float radians = yAngle * Mth.DEG_TO_RAD;
        float dx = -Mth.sin(radians);
        float dz = Mth.cos(radians);
        return this.normal.getX() * dx + this.normal.getZ() * dz > 0.0F;
    }

    public enum Axis implements Predicate<Direction>, StringRepresentable {
        X("x"),
        Y("y"),
        Z("z");

        public static final Axis[] VALUES = values();
        public static final StringRepresentable.EnumCodec<Axis> CODEC = StringRepresentable.fromEnum(Axis::values);

        private final String name;

        Axis(String name) {
            this.name = name;
        }

        public static Axis byName(String name) {
            return CODEC.byName(name);
        }

        public String getName() {
            return this.name;
        }

        public boolean isVertical() {
            return this == Y;
        }

        public boolean isHorizontal() {
            return this == X || this == Z;
        }

        public Direction getPositive() {
            return switch (this) {
                case X -> EAST;
                case Y -> UP;
                case Z -> SOUTH;
            };
        }

        public Direction getNegative() {
            return switch (this) {
                case X -> WEST;
                case Y -> DOWN;
                case Z -> NORTH;
            };
        }

        public Direction[] getDirections() {
            return new Direction[] {this.getPositive(), this.getNegative()};
        }

        @Override
        public String toString() {
            return this.name;
        }

        public static Axis getRandom(RandomSource random) {
            return Util.getRandom(VALUES, random);
        }

        @Override
        public boolean test(Direction input) {
            return input != null && input.getAxis() == this;
        }

        public Plane getPlane() {
            return this == Y ? Plane.VERTICAL : Plane.HORIZONTAL;
        }

        @Override
        public String getSerializedName() {
            return this.name;
        }

        public int choose(int x, int y, int z) {
            return switch (this) {
                case X -> x;
                case Y -> y;
                case Z -> z;
            };
        }

        public double choose(double x, double y, double z) {
            return switch (this) {
                case X -> x;
                case Y -> y;
                case Z -> z;
            };
        }

        public boolean choose(boolean x, boolean y, boolean z) {
            return switch (this) {
                case X -> x;
                case Y -> y;
                case Z -> z;
            };
        }
    }

    public enum AxisDirection {
        POSITIVE(1, "Towards positive"),
        NEGATIVE(-1, "Towards negative");

        private final int step;
        private final String name;

        AxisDirection(int step, String name) {
            this.step = step;
            this.name = name;
        }

        public int getStep() {
            return this.step;
        }

        public String getName() {
            return this.name;
        }

        @Override
        public String toString() {
            return this.name;
        }

        public AxisDirection opposite() {
            return this == POSITIVE ? NEGATIVE : POSITIVE;
        }
    }

    public enum Plane implements Predicate<Direction>, Iterable<Direction> {
        HORIZONTAL(new Direction[] {NORTH, EAST, SOUTH, WEST}, new Axis[] {Axis.X, Axis.Z}),
        VERTICAL(new Direction[] {UP, DOWN}, new Axis[] {Axis.Y});

        private final Direction[] faces;
        private final Axis[] axes;

        Plane(Direction[] faces, Axis[] axes) {
            this.faces = faces;
            this.axes = axes;
        }

        public Direction getRandomDirection(RandomSource random) {
            return Util.getRandom(this.faces, random);
        }

        public Axis getRandomAxis(RandomSource random) {
            return Util.getRandom(this.axes, random);
        }

        @Override
        public boolean test(Direction input) {
            return input != null && input.getAxis().getPlane() == this;
        }

        @Override
        public Iterator<Direction> iterator() {
            return Iterators.forArray(this.faces);
        }

        public Stream<Direction> stream() {
            return Arrays.stream(this.faces);
        }

        public List<Direction> shuffledCopy(RandomSource random) {
            return Util.shuffledCopy(this.faces, random);
        }

        public int length() {
            return this.faces.length;
        }
    }
}

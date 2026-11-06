// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import com.mojang.math.OctahedralGroup;
import it.unimi.dsi.fastutil.doubles.DoubleList;
import java.util.Map;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.properties.AttachFace;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public final class Shapes {
    private Shapes() {}

    public static final double EPSILON = 1.0E-7;
    public static final double BIG_EPSILON = 1.0E-6;
    public static final VoxelShape INFINITY = box(Double.NEGATIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY);
    public static VoxelShape empty() {
        return VoxelShape.EMPTY;
    }
    public static VoxelShape block() {
        return VoxelShape.BLOCK;
    }
    public static VoxelShape box(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        if (minX > maxX || minY > maxY || minZ > maxZ) {
            throw new IllegalArgumentException("The minimum coordinate is above the maximum");
        }
        return create(minX, minY, minZ, maxX, maxY, maxZ);
    }
    public static VoxelShape create(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        if (maxX - minX < EPSILON || maxY - minY < EPSILON || maxZ - minZ < EPSILON) {
            return VoxelShape.EMPTY;
        }
        return VoxelShape.ofBoxes(java.util.List.of(new AABB(minX, minY, minZ, maxX, maxY, maxZ)));
    }
    public static VoxelShape create(AABB aabb) {
        return create(aabb.minX, aabb.minY, aabb.minZ, aabb.maxX, aabb.maxY, aabb.maxZ);
    }
    static int findBits(double min, double max) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#findBits");
    }
    static long lcm(int first, int second) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#lcm");
    }
    public static VoxelShape or(VoxelShape first, VoxelShape second) {
        return join(first, second, BooleanOp.OR);
    }
    public static VoxelShape or(VoxelShape first, VoxelShape... tail) {
        VoxelShape result = first;
        for (VoxelShape each : tail) {
            result = join(result, each, BooleanOp.OR);
        }
        return result;
    }
    public static VoxelShape join(VoxelShape first, VoxelShape second, BooleanOp op) {
        return joinUnoptimized(first, second, op);
    }
    public static VoxelShape joinUnoptimized(VoxelShape first, VoxelShape second, BooleanOp op) {
        if (op.apply(false, false)) {
            throw new IllegalArgumentException("A boolean operation that is true for two empty shapes has no finite result");
        }
        if (first.isEmpty() && second.isEmpty()) {
            return VoxelShape.EMPTY;
        }
        if (op == BooleanOp.OR && first.isEmpty()) {
            return second;
        }
        if (op == BooleanOp.OR && second.isEmpty()) {
            return first;
        }
        return VoxelShape.ofBoxes(cells(first, second, op));
    }
    public static boolean joinIsNotEmpty(VoxelShape first, VoxelShape second, BooleanOp op) {
        return !joinUnoptimized(first, second, op).isEmpty();
    }
    public static double collide(Direction.Axis axis, AABB moving, Iterable<VoxelShape> shapes, double distance) {
        double reach = distance;
        for (VoxelShape shape : shapes) {
            for (AABB box : shape.toAabbs()) {
                reach = collideBox(axis, moving, box, reach);
                if (Math.abs(reach) < EPSILON) {
                    return 0.0;
                }
            }
        }
        return reach;
    }
    public static boolean blockOccludes(VoxelShape shape, VoxelShape occluder, Direction direction) {
        if (shape == VoxelShape.BLOCK && occluder == VoxelShape.BLOCK) {
            return true;
        }
        return faceShapeOccludes(shape.getFaceShape(direction), occluder.getFaceShape(direction.getOpposite()));
    }
    public static boolean mergedFaceOccludes(VoxelShape shape, VoxelShape occluder, Direction direction) {
        return faceShapeOccludes(shape.getFaceShape(direction), occluder.getFaceShape(direction.getOpposite()));
    }
    public static boolean faceShapeOccludes(VoxelShape shape, VoxelShape occluder) {
        return !joinIsNotEmpty(VoxelShape.BLOCK, joinUnoptimized(shape, occluder, BooleanOp.OR), BooleanOp.ONLY_FIRST);
    }
    static IndexMerger createIndexMerger(int cost, DoubleList first, DoubleList second, boolean firstOnlyMatters, boolean secondOnlyMatters) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#createIndexMerger");
    }
    public static VoxelShape rotate(VoxelShape shape, OctahedralGroup rotation) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotate");
    }
    public static VoxelShape rotate(VoxelShape shape, OctahedralGroup rotation, Vec3 rotationPoint) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotate");
    }
    static DoubleList flipAxisIfNeeded(DoubleList newAxis, boolean flip, double newRelative, double oldRelative) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#flipAxisIfNeeded");
    }
    public static boolean equal(VoxelShape first, VoxelShape second) {
        return !joinIsNotEmpty(first, second, BooleanOp.NOT_SAME);
    }
    public static Map<Direction.Axis, VoxelShape> rotateHorizontalAxis(VoxelShape zAxis) {
        return rotateHorizontalAxis(zAxis, new Vec3(0.5, 0.5, 0.5));
    }
    public static Map<Direction.Axis, VoxelShape> rotateHorizontalAxis(VoxelShape zAxis, Vec3 rotationCenter) {
        return Map.of(Direction.Axis.Z, zAxis, Direction.Axis.X, turn(zAxis, Direction.EAST));
    }
    public static Map<Direction.Axis, VoxelShape> rotateAllAxis(VoxelShape north) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAllAxis");
    }
    public static Map<Direction.Axis, VoxelShape> rotateAllAxis(VoxelShape north, Vec3 rotationCenter) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAllAxis");
    }
    public static Map<Direction, VoxelShape> rotateHorizontal(VoxelShape north) {
        return rotateHorizontal(north, null, new Vec3(0.5, 0.5, 0.5));
    }
    public static Map<Direction, VoxelShape> rotateHorizontal(VoxelShape north, OctahedralGroup initial) {
        return rotateHorizontal(north, initial, new Vec3(0.5, 0.5, 0.5));
    }
    public static Map<Direction, VoxelShape> rotateHorizontal(VoxelShape north, OctahedralGroup initial, Vec3 rotationCenter) {
        return Map.of(Direction.NORTH, north, Direction.EAST, turn(north, Direction.EAST), Direction.SOUTH, turn(north, Direction.SOUTH), Direction.WEST, turn(north, Direction.WEST));
    }
    public static Map<Direction, VoxelShape> rotateAll(VoxelShape north) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAll");
    }
    public static Map<Direction, VoxelShape> rotateAll(VoxelShape north, Vec3 rotationCenter) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAll");
    }
    public static Map<Direction, VoxelShape> rotateAll(VoxelShape north, OctahedralGroup initial, Vec3 rotationCenter) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAll");
    }
    public static Map<AttachFace, Map<Direction, VoxelShape>> rotateAttachFace(VoxelShape north) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAttachFace");
    }
    public static Map<AttachFace, Map<Direction, VoxelShape>> rotateAttachFace(VoxelShape north, OctahedralGroup initial) {
        throw new UnsupportedOperationException("net.minecraft.world.phys.shapes.Shapes#rotateAttachFace");
    }
    private static double[] cuts(VoxelShape first, VoxelShape second, int axis) {
        java.util.TreeSet<Double> values = new java.util.TreeSet<>();
        for (VoxelShape shape : java.util.List.of(first, second)) {
            for (AABB box : shape.toAabbs()) {
                values.add(axis == 0 ? box.minX : axis == 1 ? box.minY : box.minZ);
                values.add(axis == 0 ? box.maxX : axis == 1 ? box.maxY : box.maxZ);
            }
        }
        double[] result = new double[values.size()];
        int index = 0;
        for (double value : values) {
            result[index++] = value;
        }
        return result;
    }

    private static boolean inside(VoxelShape shape, double x, double y, double z) {
        for (AABB box : shape.toAabbs()) {
            if (x >= box.minX && x < box.maxX && y >= box.minY && y < box.maxY && z >= box.minZ && z < box.maxZ) {
                return true;
            }
        }
        return false;
    }

    private static java.util.List<AABB> cells(VoxelShape first, VoxelShape second, BooleanOp op) {
        double[] xs = cuts(first, second, 0);
        double[] ys = cuts(first, second, 1);
        double[] zs = cuts(first, second, 2);
        java.util.List<AABB> found = new java.util.ArrayList<>();
        for (int i = 0; i + 1 < xs.length; i++) {
            for (int j = 0; j + 1 < ys.length; j++) {
                for (int k = 0; k + 1 < zs.length; k++) {
                    double x = (xs[i] + xs[i + 1]) / 2.0;
                    double y = (ys[j] + ys[j + 1]) / 2.0;
                    double z = (zs[k] + zs[k + 1]) / 2.0;
                    if (op.apply(inside(first, x, y, z), inside(second, x, y, z))) {
                        found.add(new AABB(xs[i], ys[j], zs[k], xs[i + 1], ys[j + 1], zs[k + 1]));
                    }
                }
            }
        }
        return found;
    }

    private static double collideBox(Direction.Axis axis, AABB moving, AABB fixed, double distance) {
        Direction.Axis first = axis == Direction.Axis.X ? Direction.Axis.Y : axis == Direction.Axis.Y ? Direction.Axis.Z : Direction.Axis.X;
        Direction.Axis second = axis == Direction.Axis.X ? Direction.Axis.Z : axis == Direction.Axis.Y ? Direction.Axis.X : Direction.Axis.Y;
        if (!(moving.max(first) > fixed.min(first) + EPSILON && moving.min(first) < fixed.max(first) - EPSILON
            && moving.max(second) > fixed.min(second) + EPSILON && moving.min(second) < fixed.max(second) - EPSILON)) {
            return distance;
        }
        if (distance > 0.0 && fixed.min(axis) >= moving.max(axis) - EPSILON) {
            return Math.min(distance, fixed.min(axis) - moving.max(axis));
        }
        if (distance < 0.0 && fixed.max(axis) <= moving.min(axis) + EPSILON) {
            return Math.max(distance, fixed.max(axis) - moving.min(axis));
        }
        return distance;
    }

    private static VoxelShape turn(VoxelShape north, Direction facing) {
        java.util.List<AABB> turned = new java.util.ArrayList<>();
        for (AABB box : north.toAabbs()) {
            double[] a = turnPoint(box.minX, box.minZ, facing);
            double[] b = turnPoint(box.maxX, box.maxZ, facing);
            turned.add(new AABB(Math.min(a[0], b[0]), box.minY, Math.min(a[1], b[1]), Math.max(a[0], b[0]), box.maxY, Math.max(a[1], b[1])));
        }
        return VoxelShape.ofBoxes(turned);
    }

    private static double[] turnPoint(double x, double z, Direction facing) {
        return switch (facing) {
            case EAST -> new double[] {1.0 - z, x};
            case SOUTH -> new double[] {1.0 - x, 1.0 - z};
            case WEST -> new double[] {z, 1.0 - x};
            default -> new double[] {x, z};
        };
    }

    public interface DoubleLineConsumer {
        void consume(double x1, double y1, double z1, double x2, double y2, double z2);
    }
}

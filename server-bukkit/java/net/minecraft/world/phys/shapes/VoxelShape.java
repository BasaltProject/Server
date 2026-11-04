// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;

import it.unimi.dsi.fastutil.doubles.DoubleList;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.AxisCycle;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

public class VoxelShape {
    public static final VoxelShape EMPTY = new VoxelShape(false, true);
    public static final VoxelShape BLOCK = new VoxelShape(true, false);
    private static final VoxelShape kPartial = new VoxelShape(false, false);
    private final boolean fullBlock;
    private final boolean empty;

    private final List<AABB> boxes;

    private VoxelShape(boolean fullBlock, boolean empty) {
        this.fullBlock = fullBlock;
        this.empty = empty;
        this.boxes = fullBlock ? List.of(new AABB(0, 0, 0, 1, 1, 1)) : List.of();
    }

    private VoxelShape(List<AABB> boxes) {
        this.boxes = List.copyOf(boxes);
        this.empty = boxes.isEmpty();
        this.fullBlock = boxes.size() == 1 && boxes.get(0).minX <= 0 && boxes.get(0).minY <= 0
            && boxes.get(0).minZ <= 0 && boxes.get(0).maxX >= 1 && boxes.get(0).maxY >= 1 && boxes.get(0).maxZ >= 1;
    }

    private static Direction.Axis[] otherAxes(Direction.Axis axis) {
        return switch (axis) {
            case X -> new Direction.Axis[] {Direction.Axis.Y, Direction.Axis.Z};
            case Y -> new Direction.Axis[] {Direction.Axis.Z, Direction.Axis.X};
            case Z -> new Direction.Axis[] {Direction.Axis.X, Direction.Axis.Y};
        };
    }

    private static double component(Vec3 vec, Direction.Axis axis) {
        return switch (axis) {
            case X -> vec.x;
            case Y -> vec.y;
            case Z -> vec.z;
        };
    }

    @Override
    public int hashCode() {
        return boxes.hashCode();
    }

    public static VoxelShape ofBoxes(List<AABB> boxes) {
        return boxes.isEmpty() ? EMPTY : new VoxelShape(boxes);
    }

    public static VoxelShape of(boolean fullBlock, boolean empty) {
        return fullBlock ? BLOCK : empty ? EMPTY : kPartial;
    }

    public boolean isFullBlock() {
        return fullBlock;
    }

    public boolean isEmpty() {
        return empty;
    }
    protected final DiscreteVoxelShape shape = null;
    protected VoxelShape(DiscreteVoxelShape shape) {
        this(false, true);
    }
    public double min(Direction.Axis axis) {
        if (boxes.isEmpty()) {
            return Double.POSITIVE_INFINITY;
        }
        double found = Double.POSITIVE_INFINITY;
        for (AABB box : boxes) {
            found = Math.min(found, box.min(axis));
        }
        return found;
    }
    public double max(Direction.Axis axis) {
        if (boxes.isEmpty()) {
            return Double.NEGATIVE_INFINITY;
        }
        double found = Double.NEGATIVE_INFINITY;
        for (AABB box : boxes) {
            found = Math.max(found, box.max(axis));
        }
        return found;
    }
    public AABB bounds() {
        if (boxes.isEmpty()) {
            throw new UnsupportedOperationException("No bounds for empty shape.");
        }
        return new AABB(min(Direction.Axis.X), min(Direction.Axis.Y), min(Direction.Axis.Z), max(Direction.Axis.X),
            max(Direction.Axis.Y), max(Direction.Axis.Z));
    }
    public VoxelShape singleEncompassing() {
        return empty ? EMPTY : ofBoxes(List.of(bounds()));
    }
    protected double get(Direction.Axis axis, int i) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#get");
    }
    public DoubleList getCoords(Direction.Axis axis) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#getCoords");
    }
    public VoxelShape move(Vec3 delta) {
        return move(delta.x, delta.y, delta.z);
    }
    public VoxelShape move(Vec3i delta) {
        return move(delta.getX(), delta.getY(), delta.getZ());
    }
    public VoxelShape move(double dx, double dy, double dz) {
        return empty ? this : ofBoxes(boxes.stream().map(box -> box.move(dx, dy, dz)).toList());
    }
    public VoxelShape optimize() {
        return this;
    }
    public void forAllEdges(Shapes.DoubleLineConsumer consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#forAllEdges");
    }
    public void forAllBoxes(Shapes.DoubleLineConsumer consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#forAllBoxes");
    }
    public List<AABB> toAabbs() {
        return boxes;
    }
    public double min(Direction.Axis aAxis, double b, double c) {
        double found = Double.POSITIVE_INFINITY;
        for (AABB box : boxes) {
            Direction.Axis[] others = otherAxes(aAxis);
            if (b >= box.min(others[0]) && b < box.max(others[0]) && c >= box.min(others[1]) && c < box.max(others[1])) {
                found = Math.min(found, box.min(aAxis));
            }
        }
        return found;
    }
    public double max(Direction.Axis aAxis, double b, double c) {
        double found = Double.NEGATIVE_INFINITY;
        for (AABB box : boxes) {
            Direction.Axis[] others = otherAxes(aAxis);
            if (b >= box.min(others[0]) && b < box.max(others[0]) && c >= box.min(others[1]) && c < box.max(others[1])) {
                found = Math.max(found, box.max(aAxis));
            }
        }
        return found;
    }
    protected int findIndex(Direction.Axis axis, double coord) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#findIndex");
    }
    public BlockHitResult clip(Vec3 from, Vec3 to, BlockPos pos) {
        BlockHitResult best = null;
        double bestDistance = Double.MAX_VALUE;
        Vec3 delta = to.subtract(from);
        for (AABB local : boxes) {
            AABB box = local.move(pos.getX(), pos.getY(), pos.getZ());
            double entry = Double.NEGATIVE_INFINITY;
            double exit = Double.POSITIVE_INFINITY;
            Direction face = null;
            boolean hit = true;
            for (Direction.Axis axis : Direction.Axis.values()) {
                double origin = component(from, axis);
                double step = component(delta, axis);
                if (Math.abs(step) < 1.0E-9) {
                    if (origin < box.min(axis) || origin > box.max(axis)) {
                        hit = false;
                        break;
                    }
                    continue;
                }
                double near = (box.min(axis) - origin) / step;
                double far = (box.max(axis) - origin) / step;
                Direction nearFace = step > 0 ? Direction.get(Direction.AxisDirection.NEGATIVE, axis)
                    : Direction.get(Direction.AxisDirection.POSITIVE, axis);
                if (near > far) {
                    double swap = near;
                    near = far;
                    far = swap;
                }
                if (near > entry) {
                    entry = near;
                    face = nearFace;
                }
                exit = Math.min(exit, far);
                if (entry > exit) {
                    hit = false;
                    break;
                }
            }
            if (hit && face != null && entry >= 0 && entry <= 1 && entry < bestDistance) {
                bestDistance = entry;
                best = new BlockHitResult(from.add(delta.x * entry, delta.y * entry, delta.z * entry), face, pos, false);
            }
        }
        return best;
    }
    public Optional<Vec3> closestPointTo(Vec3 point) {
        if (empty) {
            return Optional.empty();
        }
        Vec3 best = null;
        double bestDistance = Double.MAX_VALUE;
        for (AABB box : boxes) {
            Vec3 candidate = new Vec3(Math.max(box.minX, Math.min(box.maxX, point.x)),
                Math.max(box.minY, Math.min(box.maxY, point.y)), Math.max(box.minZ, Math.min(box.maxZ, point.z)));
            double distance = candidate.distanceToSqr(point);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = candidate;
            }
        }
        return Optional.ofNullable(best);
    }
    public VoxelShape getFaceShape(Direction direction) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#getFaceShape");
    }
    protected boolean isCubeLike() {
        return fullBlock;
    }
    public double collide(Direction.Axis axis, AABB moving, double distance) {
        double result = distance;
        for (AABB box : boxes) {
            Direction.Axis[] others = otherAxes(axis);
            if (moving.max(others[0]) <= box.min(others[0]) || moving.min(others[0]) >= box.max(others[0])
                || moving.max(others[1]) <= box.min(others[1]) || moving.min(others[1]) >= box.max(others[1])) {
                continue;
            }
            if (result > 0 && box.min(axis) >= moving.max(axis)) {
                result = Math.min(result, box.min(axis) - moving.max(axis));
            } else if (result < 0 && box.max(axis) <= moving.min(axis)) {
                result = Math.max(result, box.max(axis) - moving.min(axis));
            }
        }
        return result;
    }
    protected double collideX(AxisCycle transform, AABB moving, double distance) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.phys.shapes.VoxelShape#collideX");
    }
    public boolean equals(Object obj) {
        return obj instanceof VoxelShape other && boxes.equals(other.boxes);
    }
    public String toString() {
        return empty ? "EMPTY" : "VoxelShape" + boxes;
    }
}

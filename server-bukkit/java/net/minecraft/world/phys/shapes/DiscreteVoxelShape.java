// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;

import com.mojang.math.OctahedralGroup;
import net.minecraft.core.AxisCycle;
import net.minecraft.core.Direction;

public abstract class DiscreteVoxelShape {
    protected final int xSize;
    protected final int ySize;
    protected final int zSize;

    protected DiscreteVoxelShape(int xSize, int ySize, int zSize) {
        if (xSize < 0 || ySize < 0 || zSize < 0) {
            throw new IllegalArgumentException("Need all positive sizes: x: " + xSize + ", y: " + ySize + ", z: " + zSize);
        }
        this.xSize = xSize;
        this.ySize = ySize;
        this.zSize = zSize;
    }

    public DiscreteVoxelShape rotate(OctahedralGroup rotation) {
        if (rotation == OctahedralGroup.IDENTITY) {
            return this;
        }
        throw new UnsupportedOperationException("Rotating a discrete voxel shape needs the octahedral group transforms");
    }

    public boolean isFullWide(AxisCycle transform, int x, int y, int z) {
        return isFullWide(transform.cycle(x, y, z, Direction.Axis.X), transform.cycle(x, y, z, Direction.Axis.Y),
            transform.cycle(x, y, z, Direction.Axis.Z));
    }

    public boolean isFullWide(int x, int y, int z) {
        return x >= 0 && y >= 0 && z >= 0 && x < xSize && y < ySize && z < zSize && isFull(x, y, z);
    }

    public boolean isFull(AxisCycle transform, int x, int y, int z) {
        return isFull(transform.cycle(x, y, z, Direction.Axis.X), transform.cycle(x, y, z, Direction.Axis.Y),
            transform.cycle(x, y, z, Direction.Axis.Z));
    }

    public abstract boolean isFull(int x, int y, int z);

    public abstract void fill(int x, int y, int z);

    public boolean isEmpty() {
        for (Direction.Axis axis : Direction.Axis.values()) {
            if (firstFull(axis) >= lastFull(axis)) {
                return true;
            }
        }
        return false;
    }

    public abstract int firstFull(Direction.Axis axis);

    public abstract int lastFull(Direction.Axis axis);

    public int firstFull(Direction.Axis aAxis, int b, int c) {
        int size = getSize(aAxis);
        if (b < 0 || c < 0 || b >= getSize(AxisCycle.FORWARD.cycle(aAxis)) || c >= getSize(AxisCycle.BACKWARD.cycle(aAxis))) {
            return size;
        }
        AxisCycle transform = AxisCycle.between(Direction.Axis.X, aAxis);
        for (int a = 0; a < size; a++) {
            if (isFull(transform, a, b, c)) {
                return a;
            }
        }
        return size;
    }

    public int lastFull(Direction.Axis aAxis, int b, int c) {
        if (b < 0 || c < 0 || b >= getSize(AxisCycle.FORWARD.cycle(aAxis)) || c >= getSize(AxisCycle.BACKWARD.cycle(aAxis))) {
            return 0;
        }
        AxisCycle transform = AxisCycle.between(Direction.Axis.X, aAxis);
        for (int a = getSize(aAxis) - 1; a >= 0; a--) {
            if (isFull(transform, a, b, c)) {
                return a + 1;
            }
        }
        return 0;
    }

    public int getSize(Direction.Axis axis) {
        return axis.choose(xSize, ySize, zSize);
    }

    public int getXSize() {
        return xSize;
    }

    public int getYSize() {
        return ySize;
    }

    public int getZSize() {
        return zSize;
    }

    public void forAllEdges(IntLineConsumer consumer, boolean mergeNeighbors) {
        forAllAxisEdges(consumer, AxisCycle.NONE, mergeNeighbors);
        forAllAxisEdges(consumer, AxisCycle.FORWARD, mergeNeighbors);
        forAllAxisEdges(consumer, AxisCycle.BACKWARD, mergeNeighbors);
    }

    private void forAllAxisEdges(IntLineConsumer consumer, AxisCycle transform, boolean mergeNeighbors) {
        AxisCycle inverse = transform.inverse();
        int aSize = getSize(inverse.cycle(Direction.Axis.X));
        int bSize = getSize(inverse.cycle(Direction.Axis.Y));
        int cSize = getSize(inverse.cycle(Direction.Axis.Z));
        for (int a = 0; a <= aSize; a++) {
            for (int b = 0; b <= bSize; b++) {
                int runStart = -1;
                for (int c = 0; c <= cSize; c++) {
                    int full = 0;
                    int odd = 0;
                    for (int da = 0; da <= 1; da++) {
                        for (int db = 0; db <= 1; db++) {
                            if (isFullWide(inverse, a + da - 1, b + db - 1, c)) {
                                full++;
                                odd ^= da ^ db;
                            }
                        }
                    }
                    if (full == 1 || full == 3 || (full == 2 && (odd & 1) == 0)) {
                        if (!mergeNeighbors) {
                            line(consumer, inverse, a, b, c, c + 1);
                        } else if (runStart == -1) {
                            runStart = c;
                        }
                    } else if (runStart != -1) {
                        line(consumer, inverse, a, b, runStart, c);
                        runStart = -1;
                    }
                }
            }
        }
    }

    private static void line(IntLineConsumer consumer, AxisCycle inverse, int a, int b, int c1, int c2) {
        consumer.consume(inverse.cycle(a, b, c1, Direction.Axis.X), inverse.cycle(a, b, c1, Direction.Axis.Y),
            inverse.cycle(a, b, c1, Direction.Axis.Z), inverse.cycle(a, b, c2, Direction.Axis.X),
            inverse.cycle(a, b, c2, Direction.Axis.Y), inverse.cycle(a, b, c2, Direction.Axis.Z));
    }

    public void forAllBoxes(IntLineConsumer consumer, boolean mergeNeighbors) {
        BitSetDiscreteVoxelShape.forAllBoxes(this, consumer, mergeNeighbors);
    }

    public void forAllFaces(IntFaceConsumer consumer) {
        forAllAxisFaces(consumer, AxisCycle.NONE);
        forAllAxisFaces(consumer, AxisCycle.FORWARD);
        forAllAxisFaces(consumer, AxisCycle.BACKWARD);
    }

    private void forAllAxisFaces(IntFaceConsumer consumer, AxisCycle transform) {
        AxisCycle inverse = transform.inverse();
        Direction.Axis cAxis = inverse.cycle(Direction.Axis.Z);
        int aSize = getSize(inverse.cycle(Direction.Axis.X));
        int bSize = getSize(inverse.cycle(Direction.Axis.Y));
        int cSize = getSize(cAxis);
        Direction negative = Direction.fromAxisAndDirection(cAxis, Direction.AxisDirection.NEGATIVE);
        Direction positive = Direction.fromAxisAndDirection(cAxis, Direction.AxisDirection.POSITIVE);
        for (int a = 0; a < aSize; a++) {
            for (int b = 0; b < bSize; b++) {
                boolean previous = false;
                for (int c = 0; c <= cSize; c++) {
                    boolean current = c != cSize && isFull(inverse, a, b, c);
                    if (!previous && current) {
                        consumer.consume(negative, inverse.cycle(a, b, c, Direction.Axis.X),
                            inverse.cycle(a, b, c, Direction.Axis.Y), inverse.cycle(a, b, c, Direction.Axis.Z));
                    }
                    if (previous && !current) {
                        consumer.consume(positive, inverse.cycle(a, b, c - 1, Direction.Axis.X),
                            inverse.cycle(a, b, c - 1, Direction.Axis.Y), inverse.cycle(a, b, c - 1, Direction.Axis.Z));
                    }
                    previous = current;
                }
            }
        }
    }

    public interface IntFaceConsumer {
        void consume(Direction direction, int x, int y, int z);
    }

    public interface IntLineConsumer {
        void consume(int x1, int y1, int z1, int x2, int y2, int z2);
    }
}

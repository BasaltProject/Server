// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;

import java.util.BitSet;
import net.minecraft.core.Direction;

public final class BitSetDiscreteVoxelShape extends DiscreteVoxelShape {
    private final BitSet storage;
    private int xMin;
    private int yMin;
    private int zMin;
    private int xMax;
    private int yMax;
    private int zMax;

    public BitSetDiscreteVoxelShape(int xSize, int ySize, int zSize) {
        super(xSize, ySize, zSize);
        this.storage = new BitSet(xSize * ySize * zSize);
        this.xMin = xSize;
        this.yMin = ySize;
        this.zMin = zSize;
    }

    public static BitSetDiscreteVoxelShape withFilledBounds(int xSize, int ySize, int zSize, int xMin, int yMin, int zMin,
                                                            int xMax, int yMax, int zMax) {
        BitSetDiscreteVoxelShape shape = new BitSetDiscreteVoxelShape(xSize, ySize, zSize);
        shape.xMin = xMin;
        shape.yMin = yMin;
        shape.zMin = zMin;
        shape.xMax = xMax;
        shape.yMax = yMax;
        shape.zMax = zMax;
        for (int x = xMin; x < xMax; x++) {
            for (int y = yMin; y < yMax; y++) {
                for (int z = zMin; z < zMax; z++) {
                    shape.fillUpdateBounds(x, y, z, false);
                }
            }
        }
        return shape;
    }

    public BitSetDiscreteVoxelShape(DiscreteVoxelShape source) {
        super(source.xSize, source.ySize, source.zSize);
        if (source instanceof BitSetDiscreteVoxelShape bits) {
            this.storage = (BitSet) bits.storage.clone();
        } else {
            this.storage = new BitSet(xSize * ySize * zSize);
            for (int x = 0; x < xSize; x++) {
                for (int y = 0; y < ySize; y++) {
                    for (int z = 0; z < zSize; z++) {
                        if (source.isFull(x, y, z)) {
                            storage.set(indexOf(x, y, z));
                        }
                    }
                }
            }
        }
        this.xMin = source.firstFull(Direction.Axis.X);
        this.yMin = source.firstFull(Direction.Axis.Y);
        this.zMin = source.firstFull(Direction.Axis.Z);
        this.xMax = source.lastFull(Direction.Axis.X);
        this.yMax = source.lastFull(Direction.Axis.Y);
        this.zMax = source.lastFull(Direction.Axis.Z);
    }

    private int indexOf(int x, int y, int z) {
        return (x * ySize + y) * zSize + z;
    }

    @Override
    public boolean isFull(int x, int y, int z) {
        return storage.get(indexOf(x, y, z));
    }

    private void fillUpdateBounds(int x, int y, int z, boolean updateBounds) {
        storage.set(indexOf(x, y, z));
        if (updateBounds) {
            xMin = Math.min(xMin, x);
            yMin = Math.min(yMin, y);
            zMin = Math.min(zMin, z);
            xMax = Math.max(xMax, x + 1);
            yMax = Math.max(yMax, y + 1);
            zMax = Math.max(zMax, z + 1);
        }
    }

    @Override
    public void fill(int x, int y, int z) {
        fillUpdateBounds(x, y, z, true);
    }

    @Override
    public boolean isEmpty() {
        return storage.isEmpty();
    }

    @Override
    public int firstFull(Direction.Axis axis) {
        return axis.choose(xMin, yMin, zMin);
    }

    @Override
    public int lastFull(Direction.Axis axis) {
        return axis.choose(xMax, yMax, zMax);
    }

    public static BitSetDiscreteVoxelShape join(DiscreteVoxelShape first, DiscreteVoxelShape second, IndexMerger xMerger,
                                                IndexMerger yMerger, IndexMerger zMerger, BooleanOp op) {
        throw new UnsupportedOperationException("Joining discrete voxel shapes needs the index mergers");
    }

    static void forAllBoxes(DiscreteVoxelShape shape, DiscreteVoxelShape.IntLineConsumer consumer, boolean mergeNeighbors) {
        BitSetDiscreteVoxelShape work = new BitSetDiscreteVoxelShape(shape);
        for (int y = 0; y < work.ySize; y++) {
            for (int x = 0; x < work.xSize; x++) {
                int runStart = -1;
                for (int z = 0; z <= work.zSize; z++) {
                    if (work.isFullWide(x, y, z)) {
                        if (mergeNeighbors) {
                            if (runStart == -1) {
                                runStart = z;
                            }
                        } else {
                            consumer.consume(x, y, z, x + 1, y + 1, z + 1);
                        }
                    } else if (runStart != -1) {
                        int endX = x + 1;
                        while (endX < work.xSize && work.rowFull(endX, y, runStart, z)) {
                            work.clearRow(endX, y, runStart, z);
                            endX++;
                        }
                        int endY = y + 1;
                        while (endY < work.ySize && work.slabFull(x, endX, endY, runStart, z)) {
                            work.clearSlab(x, endX, endY, runStart, z);
                            endY++;
                        }
                        consumer.consume(x, y, runStart, endX, endY, z);
                        runStart = -1;
                    }
                }
            }
        }
    }

    private boolean rowFull(int x, int y, int from, int to) {
        for (int z = from; z < to; z++) {
            if (!isFull(x, y, z)) {
                return false;
            }
        }
        return true;
    }

    private void clearRow(int x, int y, int from, int to) {
        storage.clear(indexOf(x, y, from), indexOf(x, y, to - 1) + 1);
    }

    private boolean slabFull(int xFrom, int xTo, int y, int zFrom, int zTo) {
        for (int x = xFrom; x < xTo; x++) {
            if (!rowFull(x, y, zFrom, zTo)) {
                return false;
            }
        }
        return true;
    }

    private void clearSlab(int xFrom, int xTo, int y, int zFrom, int zTo) {
        for (int x = xFrom; x < xTo; x++) {
            clearRow(x, y, zFrom, zTo);
        }
    }

    public boolean isInterior(int x, int y, int z) {
        return isFullWide(x, y, z) && isFullWide(x - 1, y, z) && isFullWide(x + 1, y, z) && isFullWide(x, y - 1, z)
            && isFullWide(x, y + 1, z) && isFullWide(x, y, z - 1) && isFullWide(x, y, z + 1);
    }
}

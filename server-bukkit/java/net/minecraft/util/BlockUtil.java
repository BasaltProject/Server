// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.datafixers.util.Pair;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

public class BlockUtil {
    public static final int MAX_POSITION_DIFFERENCE_PACKING_RADIUS = 127;

    public static FoundRectangle getLargestRectangleAround(BlockPos center, Direction.Axis axis1, int limit1, Direction.Axis axis2, int limit2,
            Predicate<BlockPos> test) {
        BlockPos.MutableBlockPos cursor = center.mutable();
        Direction negative1 = Direction.get(Direction.AxisDirection.NEGATIVE, axis1);
        Direction positive1 = negative1.getOpposite();
        Direction negative2 = Direction.get(Direction.AxisDirection.NEGATIVE, axis2);
        Direction positive2 = negative2.getOpposite();
        int below1 = reach(test, cursor.set(center), negative1, limit1);
        int above1 = reach(test, cursor.set(center), positive1, limit1);
        IntBounds[] rows = new IntBounds[below1 + 1 + above1];
        rows[below1] = new IntBounds(reach(test, cursor.set(center), negative2, limit2), reach(test, cursor.set(center), positive2, limit2));
        int centerIndex2 = rows[below1].min;
        for (int step = 1; step <= below1; step++) {
            IntBounds previous = rows[below1 - (step - 1)];
            rows[below1 - step] = new IntBounds(reach(test, cursor.set(center).move(negative1, step), negative2, previous.min),
                reach(test, cursor.set(center).move(negative1, step), positive2, previous.max));
        }
        for (int step = 1; step <= above1; step++) {
            IntBounds previous = rows[below1 + step - 1];
            rows[below1 + step] = new IntBounds(reach(test, cursor.set(center).move(positive1, step), negative2, previous.min),
                reach(test, cursor.set(center).move(positive1, step), positive2, previous.max));
        }
        int bestMin1 = 0;
        int bestMin2 = 0;
        int bestSize1 = 0;
        int bestSize2 = 0;
        int[] heights = new int[rows.length];
        for (int index2 = centerIndex2; index2 >= 0; index2--) {
            for (int index1 = 0; index1 < rows.length; index1++) {
                int low = centerIndex2 - rows[index1].min;
                int high = centerIndex2 + rows[index1].max;
                heights[index1] = index2 >= low && index2 <= high ? high + 1 - index2 : 0;
            }
            Pair<IntBounds, Integer> rectangle = getMaxRectangleLocation(heights);
            IntBounds span = rectangle.getFirst();
            int size1 = 1 + span.max - span.min;
            int size2 = rectangle.getSecond();
            if (size1 * size2 > bestSize1 * bestSize2) {
                bestMin1 = span.min;
                bestMin2 = index2;
                bestSize1 = size1;
                bestSize2 = size2;
            }
        }
        return new FoundRectangle(center.relative(axis1, bestMin1 - below1).relative(axis2, bestMin2 - centerIndex2), bestSize1, bestSize2);
    }

    private static int reach(Predicate<BlockPos> test, BlockPos.MutableBlockPos cursor, Direction direction, int limit) {
        int steps = 0;
        while (steps < limit && test.test(cursor.move(direction))) {
            steps++;
        }
        return steps;
    }

    static Pair<IntBounds, Integer> getMaxRectangleLocation(int[] columns) {
        int bestStart = 0;
        int bestEnd = 0;
        int bestHeight = 0;
        Deque<Integer> stack = new ArrayDeque<>();
        stack.push(0);
        for (int column = 1; column <= columns.length; column++) {
            int height = column == columns.length ? 0 : columns[column];
            while (!stack.isEmpty()) {
                int stackHeight = columns[stack.peek()];
                if (height >= stackHeight) {
                    stack.push(column);
                    break;
                }
                stack.pop();
                int start = stack.isEmpty() ? 0 : stack.peek() + 1;
                if (stackHeight * (column - start) > bestHeight * (bestEnd - bestStart)) {
                    bestEnd = column;
                    bestStart = start;
                    bestHeight = stackHeight;
                }
            }
            if (stack.isEmpty()) {
                stack.push(column);
            }
        }
        return new Pair<>(new IntBounds(bestStart, bestEnd - 1), bestHeight);
    }

    public static Optional<BlockPos> getTopConnectedBlock(BlockGetter level, BlockPos pos, Block bodyBlock, Direction growthDirection, Block headBlock) {
        BlockPos.MutableBlockPos cursor = pos.mutable();
        BlockState state;
        do {
            cursor.move(growthDirection);
            state = level.getBlockState(cursor);
        } while (state.is(bodyBlock));
        return state.is(headBlock) ? Optional.of(cursor) : Optional.empty();
    }

    public static int clampedPackDifferenceInPosition(BlockPos pos, BlockPos testPos, int xRadius, int yRadius, int zRadius) {
        return pack(Mth.clamp(xRadius, 0, 127), Mth.clamp(yRadius, 0, 127), Mth.clamp(zRadius, 0, 127),
            Mth.clamp(testPos.getX() - pos.getX(), -127, 127), Mth.clamp(testPos.getY() - pos.getY(), -127, 127),
            Mth.clamp(testPos.getZ() - pos.getZ(), -127, 127));
    }

    public static int packDifferenceInPosition(BlockPos pos, BlockPos testPos, int xRadius, int yRadius, int zRadius) {
        if (xRadius > 127 || yRadius > 127 || zRadius > 127) {
            throw new IllegalArgumentException("The radius cannot be larger than 127 when packing the difference between two positions");
        }
        return pack(xRadius, yRadius, zRadius, boundedDifference(testPos.getX(), pos.getX()), boundedDifference(testPos.getY(), pos.getY()),
            boundedDifference(testPos.getZ(), pos.getZ()));
    }

    private static int pack(int xRadius, int yRadius, int zRadius, int xDiff, int yDiff, int zDiff) {
        return (xDiff + xRadius & 255) << 16 | (yDiff + yRadius & 255) << 8 | zDiff + zRadius & 255;
    }

    private static int boundedDifference(int from, int to) {
        int difference = from - to;
        if (difference > 127 || difference < -127) {
            throw new IllegalArgumentException("The difference must be within [127,-127]");
        }
        return difference;
    }

    public static BlockPos unpackDifferenceInPosition(BlockPos pos, int packedDiff, int xRadius, int yRadius, int zRadius) {
        if (xRadius > 127 || yRadius > 127 || zRadius > 127) {
            throw new IllegalArgumentException("The radius cannot be larger than 127 when unpacking the difference between two positions.");
        }
        return pos.offset((packedDiff >> 16 & 255) - xRadius, (packedDiff >> 8 & 255) - yRadius, (packedDiff & 255) - zRadius);
    }

    public static class IntBounds {
        public final int min;
        public final int max;

        public IntBounds(int min, int max) {
            this.min = min;
            this.max = max;
        }

        @Override
        public String toString() {
            return "IntBounds{min=" + min + ", max=" + max + "}";
        }
    }

    public static class FoundRectangle {
        public final BlockPos minCorner;
        public final int axis1Size;
        public final int axis2Size;

        public FoundRectangle(BlockPos minCorner, int axis1Size, int axis2Size) {
            this.minCorner = minCorner;
            this.axis1Size = axis1Size;
            this.axis2Size = axis2Size;
        }
    }
}

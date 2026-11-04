package org.basaltmc.bukkit.facade;

import java.util.ArrayDeque;
import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.Queue;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.BlockPos.MutableBlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.phys.Vec3;

public final class BlockPosIterators {
    private BlockPosIterators() {}

    private abstract static class Lazy<T> implements Iterator<T> {
        private T pending;
        private boolean done;

        abstract T advance();

        protected final T finish() {
            done = true;
            return null;
        }

        @Override
        public boolean hasNext() {
            if (pending == null && !done) {
                pending = advance();
            }
            return pending != null;
        }

        @Override
        public T next() {
            if (!hasNext()) {
                throw new NoSuchElementException();
            }
            T result = pending;
            pending = null;
            return result;
        }
    }

    public static Iterable<BlockPos> random(RandomSource random, int limit, int minX, int minY, int minZ, int maxX,
        int maxY, int maxZ) {
        int width = maxX - minX + 1;
        int height = maxY - minY + 1;
        int depth = maxZ - minZ + 1;
        return () -> new Lazy<BlockPos>() {
            private final MutableBlockPos cursor = new MutableBlockPos();
            private int remaining = limit;

            @Override
            BlockPos advance() {
                if (remaining <= 0) {
                    return finish();
                }
                remaining--;
                return cursor.set(minX + random.nextInt(width), minY + random.nextInt(height),
                    minZ + random.nextInt(depth));
            }
        };
    }

    public static Iterable<BlockPos> manhattan(BlockPos origin, int reachX, int reachY, int reachZ, int maxDepth) {
        return () -> new Lazy<BlockPos>() {
            private final MutableBlockPos cursor = new MutableBlockPos();
            private int depth;
            private int maxXAtDepth;
            private int maxYAtColumn;
            private int x;
            private int y;
            private boolean mirrorPending;

            @Override
            BlockPos advance() {
                if (mirrorPending) {
                    mirrorPending = false;
                    cursor.setZ(origin.getZ() - (cursor.getZ() - origin.getZ()));
                    return cursor;
                }
                while (true) {
                    if (y > maxYAtColumn) {
                        x++;
                        if (x > maxXAtDepth) {
                            depth++;
                            if (depth > maxDepth) {
                                return finish();
                            }
                            maxXAtDepth = Math.min(reachX, depth);
                            x = -maxXAtDepth;
                        }
                        maxYAtColumn = Math.min(reachY, depth - Math.abs(x));
                        y = -maxYAtColumn;
                    }
                    int currentY = y++;
                    int z = depth - Math.abs(x) - Math.abs(currentY);
                    if (z <= reachZ) {
                        mirrorPending = z != 0;
                        return cursor.set(origin.getX() + x, origin.getY() + currentY, origin.getZ() + z);
                    }
                }
            }
        };
    }

    public static Iterable<MutableBlockPos> spiral(BlockPos center, int radius, Direction first, Direction second) {
        if (first.getAxis() == second.getAxis()) {
            throw new IllegalStateException("The two directions cannot be on the same axis");
        }
        Direction[] directions = {first, second, first.getOpposite(), second.getOpposite()};
        return () -> new Lazy<MutableBlockPos>() {
            private final MutableBlockPos cursor = center.mutable().move(second);
            private final int legs = 4 * radius;
            private int leg = -1;
            private int legSize;
            private int legIndex;
            private int lastX = cursor.getX();
            private int lastY = cursor.getY();
            private int lastZ = cursor.getZ();

            @Override
            MutableBlockPos advance() {
                cursor.set(lastX, lastY, lastZ).move(directions[(leg + 4) % 4]);
                lastX = cursor.getX();
                lastY = cursor.getY();
                lastZ = cursor.getZ();
                if (legIndex >= legSize) {
                    if (leg >= legs) {
                        return finish();
                    }
                    leg++;
                    legIndex = 0;
                    legSize = leg / 2 + 1;
                }
                legIndex++;
                return cursor;
            }
        };
    }

    private record Node(BlockPos pos, int depth) {}

    public static int breadthFirst(BlockPos start, int maxDepth, int maxCount,
        BiConsumer<BlockPos, Consumer<BlockPos>> neighbours, Function<BlockPos, BlockPos.TraversalNodeStatus> process) {
        Queue<Node> queue = new ArrayDeque<>();
        it.unimi.dsi.fastutil.longs.LongSet seen = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
        queue.add(new Node(start, 0));
        int count = 0;
        while (!queue.isEmpty()) {
            Node node = queue.poll();
            if (!seen.add(node.pos().asLong())) {
                continue;
            }
            BlockPos.TraversalNodeStatus status = process.apply(node.pos());
            if (status == BlockPos.TraversalNodeStatus.SKIP) {
                continue;
            }
            if (status == BlockPos.TraversalNodeStatus.STOP) {
                break;
            }
            if (++count >= maxCount) {
                return count;
            }
            if (node.depth() < maxDepth) {
                neighbours.accept(node.pos(), next -> queue.add(new Node(next, node.depth() + 1)));
            }
        }
        return count;
    }

    public static Iterable<BlockPos> cornersInDirection(int ax, int ay, int az, int bx, int by, int bz, Vec3 direction) {
        int minX = Math.min(ax, bx);
        int minY = Math.min(ay, by);
        int minZ = Math.min(az, bz);
        int maxX = Math.max(ax, bx);
        int maxY = Math.max(ay, by);
        int maxZ = Math.max(az, bz);
        int startX = direction.x >= 0.0 ? minX : maxX;
        int startY = direction.y >= 0.0 ? minY : maxY;
        int startZ = direction.z >= 0.0 ? minZ : maxZ;
        var axes = Direction.axisStepOrder(direction);
        Direction[] visit = new Direction[3];
        int[] extent = new int[3];
        for (int i = 0; i < 3; i++) {
            Direction.Axis axis = axes.get(i);
            visit[i] = direction.get(axis) >= 0.0 ? axis.getPositive() : axis.getNegative();
            extent[i] = axis.choose(maxX - minX, maxY - minY, maxZ - minZ);
        }
        return () -> new Lazy<BlockPos>() {
            private final MutableBlockPos cursor = new MutableBlockPos();
            private final int[] index = new int[3];
            private boolean last;

            @Override
            BlockPos advance() {
                if (last) {
                    return finish();
                }
                cursor.set(
                    startX + visit[0].getStepX() * index[0] + visit[1].getStepX() * index[1] + visit[2].getStepX() * index[2],
                    startY + visit[0].getStepY() * index[0] + visit[1].getStepY() * index[1] + visit[2].getStepY() * index[2],
                    startZ + visit[0].getStepZ() * index[0] + visit[1].getStepZ() * index[1] + visit[2].getStepZ() * index[2]);
                if (index[2] < extent[2]) {
                    index[2]++;
                } else if (index[1] < extent[1]) {
                    index[1]++;
                    index[2] = 0;
                } else if (index[0] < extent[0]) {
                    index[0]++;
                    index[1] = 0;
                    index[2] = 0;
                } else {
                    last = true;
                }
                return cursor;
            }
        };
    }
}

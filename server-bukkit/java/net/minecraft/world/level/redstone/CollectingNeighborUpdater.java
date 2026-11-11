// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;
import java.util.ArrayDeque;
import java.util.List;
import java.util.ArrayList;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class CollectingNeighborUpdater implements NeighborUpdater {
    private void addAndRun(BlockPos pos, NeighborUpdates update) {
        boolean running = count > 0;
        boolean overflow = maxChained >= 0 && count >= maxChained;
        count++;
        if (!overflow) {
            if (running) {
                addedThisLayer.add(update);
            } else {
                stack.push(update);
            }
        } else if (count - 1 == maxChained) {
            org.slf4j.LoggerFactory.getLogger(CollectingNeighborUpdater.class).error("Too many chained neighbor updates. Skipping the rest. First skipped position: {}", pos.toShortString());
        }
        if (!running) {
            drain();
        }
    }
    private void drain() {
        try {
            while (!stack.isEmpty() || !addedThisLayer.isEmpty()) {
                for (int at = addedThisLayer.size() - 1; at >= 0; at--) {
                    stack.push(addedThisLayer.get(at));
                }
                addedThisLayer.clear();
                NeighborUpdates top = stack.peek();
                if (debugListener != null) {
                    top.forEachUpdatedPos(debugListener);
                }
                while (addedThisLayer.isEmpty()) {
                    if (!top.runNext(level)) {
                        stack.pop();
                        break;
                    }
                }
            }
        } finally {
            stack.clear();
            addedThisLayer.clear();
            count = 0;
        }
    }
    private final Level level;
    private final int maxChained;
    private final ArrayDeque<NeighborUpdates> stack = new ArrayDeque<>();
    private final List<NeighborUpdates> addedThisLayer = new ArrayList<>();
    private int count;
    private Consumer<BlockPos> debugListener;
    public CollectingNeighborUpdater(Level level, int maxChainedNeighborUpdates) {
        this.level = level;
        this.maxChained = maxChainedNeighborUpdates;
    }
    public void setDebugListener(Consumer<BlockPos> debugListener) {
        this.debugListener = debugListener;
    }
    public void shapeUpdate(Direction direction, BlockState neighborState, BlockPos pos, BlockPos neighborPos, int updateFlags, int updateLimit) {
        addAndRun(pos, new ShapeUpdate(direction, neighborState, pos.immutable(), neighborPos.immutable(), updateFlags, updateLimit));
    }
    public void neighborChanged(BlockPos pos, Block block, Orientation orientation) {
        addAndRun(pos, new SimpleNeighborUpdate(pos, block, orientation));
    }
    public void neighborChanged(BlockState state, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) {
        addAndRun(pos, new FullNeighborUpdate(state, pos.immutable(), block, orientation, movedByPiston));
    }
    public void updateNeighborsAtExceptFromFacing(BlockPos pos, Block block, Direction skipDirection, Orientation orientation) {
        addAndRun(pos, new MultiNeighborUpdate(pos.immutable(), block, orientation, skipDirection));
    }
    static record SimpleNeighborUpdate(BlockPos pos, Block block, Orientation orientation) implements NeighborUpdates {
        public boolean runNext(Level level) {
        NeighborUpdater.executeUpdate(level, level.getBlockState(pos), pos, block, orientation, false);
        return false;
        }
        public void forEachUpdatedPos(Consumer<BlockPos> output) {
        output.accept(pos);
        }
    }
    static record FullNeighborUpdate(BlockState state, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) implements NeighborUpdates {
        public boolean runNext(Level level) {
        NeighborUpdater.executeUpdate(level, state, pos, block, orientation, movedByPiston);
        return false;
        }
        public void forEachUpdatedPos(Consumer<BlockPos> output) {
        output.accept(pos);
        }
    }
static final class MultiNeighborUpdate implements NeighborUpdates {
        private final BlockPos sourcePos;
        private final Block sourceBlock;
        private Orientation orientation;
        private final Direction skipDirection;
        private int next;
        public MultiNeighborUpdate(BlockPos sourcePos, Block sourceBlock, Orientation orientation, Direction skipDirection) {
        this.sourcePos = sourcePos;
        this.sourceBlock = sourceBlock;
        this.orientation = orientation;
        this.skipDirection = skipDirection;
        if (NeighborUpdater.UPDATE_ORDER[0] == skipDirection) {
            next = 1;
        }
        }
        public boolean runNext(Level level) {
        Direction side = NeighborUpdater.UPDATE_ORDER[next++];
        BlockPos neighborPos = sourcePos.relative(side);
        NeighborUpdater.executeUpdate(level, level.getBlockState(neighborPos), neighborPos, sourceBlock, null, false);
        if (next < NeighborUpdater.UPDATE_ORDER.length && NeighborUpdater.UPDATE_ORDER[next] == skipDirection) {
            next++;
        }
        return next < NeighborUpdater.UPDATE_ORDER.length;
        }
        public void forEachUpdatedPos(Consumer<BlockPos> output) {
        for (Direction side : NeighborUpdater.UPDATE_ORDER) {
            if (side != skipDirection) {
                output.accept(sourcePos.relative(side));
            }
        }
        }
    }
    static record ShapeUpdate(Direction direction, BlockState neighborState, BlockPos pos, BlockPos neighborPos, int updateFlags, int updateLimit) implements NeighborUpdates {
        public boolean runNext(Level level) {
        NeighborUpdater.executeShapeUpdate(level, direction, pos, neighborPos, neighborState, updateFlags, updateLimit);
        return false;
        }
        public void forEachUpdatedPos(Consumer<BlockPos> output) {
        output.accept(pos);
        }
    }
    interface NeighborUpdates {
        boolean runNext(Level level);
        void forEachUpdatedPos(Consumer<BlockPos> output);
    }
}

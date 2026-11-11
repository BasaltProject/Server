// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.LinkedList;
import java.util.function.Consumer;
import java.util.function.IntFunction;
import java.util.List;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.SideChainPart;
public interface SideChainPartBlock {
    private static Neighbor neighborAt(Neighbors around, Direction side, int steps) {
        return around.cache().computeIfAbsent(around.center().relative(side, steps), pos -> {
            BlockState there = around.level().getBlockState(pos);
            boolean linkable = around.block().isConnectable(there) && around.block().getFacing(there) == around.facing();
            return linkable ? new SideChainNeighbor(around.level(), around.block(), pos, around.block().getSideChainPart(there)) : new EmptyNeighbor(pos);
        });
    }
    private void collectTowards(IntFunction<Neighbor> at, SideChainPart endPart, Consumer<BlockPos> sink) {
        for (int steps = 1; steps < getMaxChainLength(); steps++) {
            Neighbor next = at.apply(steps);
            if (next.connectsTowards(endPart)) {
                sink.accept(next.pos());
            }
            if (next.isUnconnectableOrChainEnd()) {
                break;
            }
        }
    }
    private boolean canConnect(int newBlocks, int currentLength) {
        return newBlocks > 0 && currentLength + newBlocks <= getMaxChainLength();
    }
    private void setPart(LevelAccessor level, BlockPos pos, SideChainPart part) {
        BlockState state = level.getBlockState(pos);
        if (getSideChainPart(state) != part) {
            level.setBlockAndUpdate(pos, setSideChainPart(state, part));
        }
    }
    private Neighbors neighborsOf(LevelAccessor level, BlockPos center, Direction facing) {
        return new Neighbors(this, level, facing, center, new java.util.HashMap<>());
    }
    SideChainPart getSideChainPart(BlockState state);
    BlockState setSideChainPart(BlockState state, SideChainPart newPart);
    Direction getFacing(BlockState state);
    boolean isConnectable(BlockState state);
    int getMaxChainLength();
    default List<BlockPos> getAllBlocksConnectedTo(LevelAccessor level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        if (!isConnectable(state)) {
            return List.of();
        }
        Neighbors around = neighborsOf(level, pos, getFacing(state));
        LinkedList<BlockPos> chain = new LinkedList<>();
        chain.add(pos);
        collectTowards(around::left, SideChainPart.LEFT, chain::addFirst);
        collectTowards(around::right, SideChainPart.RIGHT, chain::addLast);
        return chain;
    }
    default void updateNeighborsAfterPoweringDown(LevelAccessor level, BlockPos pos, BlockState state) {
        Neighbors around = neighborsOf(level, pos, getFacing(state));
        around.left().disconnectFromRight();
        around.right().disconnectFromLeft();
    }
    default void updateSelfAndNeighborsOnPoweringUp(LevelAccessor level, BlockPos pos, BlockState state, BlockState oldState) {
        if (!isConnectable(state)) {
            return;
        }
        boolean wasLinked = getSideChainPart(state).isConnected() || isConnectable(oldState) && getSideChainPart(oldState).isConnected();
        if (wasLinked) {
            return;
        }
        Neighbors around = neighborsOf(level, pos, getFacing(state));
        SideChainPart part = SideChainPart.UNCONNECTED;
        int leftChain = around.left().isConnectable() ? getAllBlocksConnectedTo(level, around.left().pos()).size() : 0;
        int rightChain = around.right().isConnectable() ? getAllBlocksConnectedTo(level, around.right().pos()).size() : 0;
        int length = 1;
        if (canConnect(leftChain, length)) {
            part = part.whenConnectedToTheLeft();
            around.left().connectToTheRight();
            length += leftChain;
        }
        if (canConnect(rightChain, length)) {
            part = part.whenConnectedToTheRight();
            around.right().connectToTheLeft();
        }
        setPart(level, pos, part);
    }
    public static record Neighbors(SideChainPartBlock block, LevelAccessor level, Direction facing, BlockPos center, Map<BlockPos, Neighbor> cache) {
        public Neighbor left(int steps) {
            return neighborAt(this, facing.getClockWise(), steps);
        }
        public Neighbor right(int steps) {
            return neighborAt(this, facing.getCounterClockWise(), steps);
        }
        public Neighbor left() {
            return left(1);
        }
        public Neighbor right() {
            return right(1);
        }
    }
    public interface Neighbor {
        BlockPos pos();
        boolean isConnectable();
        boolean isUnconnectableOrChainEnd();
        boolean connectsTowards(SideChainPart endPart);
        default void connectToTheRight() {
            return;
        }
        default void connectToTheLeft() {
            return;
        }
        default void disconnectFromRight() {
            return;
        }
        default void disconnectFromLeft() {
            return;
        }
    }
    public static record EmptyNeighbor(BlockPos pos) implements Neighbor {
        public boolean isConnectable() {
            return false;
        }
        public boolean isUnconnectableOrChainEnd() {
            return true;
        }
        public boolean connectsTowards(SideChainPart endPart) {
            return false;
        }
    }
    public static record SideChainNeighbor(LevelAccessor level, SideChainPartBlock block, BlockPos pos, SideChainPart part) implements Neighbor {
        public boolean isConnectable() {
            return true;
        }
        public boolean isUnconnectableOrChainEnd() {
            return part.isChainEnd();
        }
        public boolean connectsTowards(SideChainPart endPart) {
            return part.isConnectionTowards(endPart);
        }
        public void connectToTheRight() {
            block.setPart(level, pos, part.whenConnectedToTheRight());
        }
        public void connectToTheLeft() {
            block.setPart(level, pos, part.whenConnectedToTheLeft());
        }
        public void disconnectFromRight() {
            block.setPart(level, pos, part.whenDisconnectedFromTheRight());
        }
        public void disconnectFromLeft() {
            block.setPart(level, pos, part.whenDisconnectedFromTheLeft());


        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.blockscan;

import java.util.function.BiConsumer;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Continuation;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;

public class BoxBlockMatcher extends BlockMatcher {
    private final BlockPos from;
    private final BlockPos to;

    public BoxBlockMatcher(LevelReader level, BlockPos from, BlockPos to) {
        super(level);
        this.from = new BlockPos(Math.min(from.getX(), to.getX()), Math.min(from.getY(), to.getY()), Math.min(from.getZ(), to.getZ()));
        this.to = new BlockPos(Math.max(from.getX(), to.getX()), Math.max(from.getY(), to.getY()), Math.max(from.getZ(), to.getZ()));
    }

    @Override
    public BoxBlockMatcher filterState(Predicate<BlockState> predicate) {
        super.filterState(predicate);
        return this;
    }

    @Override
    public boolean atLeastMatched(int n) {
        if (n <= 0) {
            return true;
        }
        int[] seen = {0};
        BlockScanUtils.findBlocks(level, from, to, statePredicate, (pos, state) -> ++seen[0] >= n ? Continuation.ABORT : Continuation.CONTINUE);
        return seen[0] >= n;
    }

    @Override
    protected boolean anyMatched(Predicate<BlockState> predicate) {
        return !BlockScanUtils.findBlocks(level, from, to, predicate, (pos, state) -> Continuation.ABORT);
    }

    @Override
    public void forEach(BiConsumer<BlockPos, BlockState> consumer) {
        BlockScanUtils.findBlocks(level, from, to, statePredicate, (pos, state) -> {
            consumer.accept(pos.immutable(), state);
            return Continuation.CONTINUE;
        });
    }

    @Override
    public boolean forEachUntil(BlockStateConsumer consumer) {
        return BlockScanUtils.findBlocks(level, from, to, statePredicate, consumer);
    }
}

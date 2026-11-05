// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.blockscan;

import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.BiPredicate;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Continuation;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;

public class OrderedBlockMatcher extends BlockMatcher {
    private Iterable<BlockPos> positions;

    public OrderedBlockMatcher(LevelReader level, Iterable<BlockPos> positions) {
        super(level);
        this.positions = positions;
    }

    public OrderedBlockMatcher filterPos(Predicate<? super BlockPos> predicate) {
        Iterable<BlockPos> source = positions;
        positions = () -> java.util.stream.StreamSupport.stream(source.spliterator(), false).filter(predicate).iterator();
        return this;
    }

    @Override
    public OrderedBlockMatcher filterState(Predicate<BlockState> predicate) {
        super.filterState(predicate);
        return this;
    }

    @Override
    public boolean atLeastMatched(int n) {
        if (n <= 0) {
            return true;
        }
        int[] seen = {0};
        BlockScanUtils.findBlocksWithCache(level, positions, statePredicate, (pos, state) -> ++seen[0] >= n ? Continuation.ABORT : Continuation.CONTINUE);
        return seen[0] >= n;
    }

    @Override
    protected boolean anyMatched(Predicate<BlockState> predicate) {
        return !BlockScanUtils.findBlocksWithCache(level, positions, predicate, (pos, state) -> Continuation.ABORT);
    }

    @Override
    public void forEach(BiConsumer<BlockPos, BlockState> consumer) {
        BlockScanUtils.findBlocksWithCache(level, positions, statePredicate, (pos, state) -> {
            consumer.accept(pos.immutable(), state);
            return Continuation.CONTINUE;
        });
    }

    @Override
    public boolean forEachUntil(BlockStateConsumer consumer) {
        return BlockScanUtils.findBlocksWithCache(level, positions, statePredicate, consumer);
    }

    public Optional<BlockPos> findFirst() {
        return findFirst((pos, state) -> true);
    }

    public Optional<BlockPos> findFirst(BiPredicate<BlockPos, BlockState> predicate) {
        BlockPos[] found = {null};
        BlockScanUtils.findBlocksWithCache(level, positions, statePredicate, (pos, state) -> {
            if (!predicate.test(pos, state)) {
                return Continuation.CONTINUE;
            }
            found[0] = pos.immutable();
            return Continuation.ABORT;
        });
        return Optional.ofNullable(found[0]);
    }
}

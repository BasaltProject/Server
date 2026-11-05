// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.blockscan;

import java.util.function.BiConsumer;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;

public abstract class BlockMatcher {
    protected final LevelReader level;
    protected Predicate<BlockState> statePredicate = state -> true;

    public BlockMatcher(LevelReader levelReader) {
        this.level = levelReader;
    }

    public BlockMatcher filterState(Predicate<BlockState> predicate) {
        statePredicate = statePredicate.and(predicate);
        return this;
    }

    public abstract boolean atLeastMatched(int n);

    public boolean atMostMatched(int n) {
        return !atLeastMatched(n + 1);
    }

    public boolean anyMatched() {
        return anyMatched(statePredicate);
    }

    protected abstract boolean anyMatched(Predicate<BlockState> statePredicate);

    public boolean noneMatched() {
        return !anyMatched();
    }

    public boolean allMatched() {
        return !anyMatched(statePredicate.negate());
    }

    public abstract void forEach(BiConsumer<BlockPos, BlockState> consumer);

    public abstract boolean forEachUntil(BlockStateConsumer consumer);
}

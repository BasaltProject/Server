// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
public class WorldGenTickAccess<T> implements LevelTickAccess<T> {
    private final Function<BlockPos, TickContainerAccess<T>> lookup;
    public WorldGenTickAccess(Function<BlockPos, TickContainerAccess<T>> containerGetter) {
        this.lookup = containerGetter;
    }
    public boolean hasScheduledTick(BlockPos pos, T type) {
        return lookup.apply(pos).hasScheduledTick(pos, type);
    }
    public void schedule(ScheduledTick<T> tick) {
        lookup.apply(tick.pos()).schedule(tick);
    }
    public boolean willTickThisTick(BlockPos pos, T type) {
        return false;
    }
    public int count() {
        return 0;
    }
}

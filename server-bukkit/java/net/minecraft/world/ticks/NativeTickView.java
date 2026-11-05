// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;

import java.util.function.LongSupplier;
import net.minecraft.core.BlockPos;
import org.basaltmc.bukkit.WorldNatives;

/** A chunk's tick queue: the scheduling itself lives in the native world, this only forwards to it. */
public class NativeTickView<T> implements TickContainerAccess<T> {
    private final String world;
    private final boolean fluid;
    private final LongSupplier gameTime;

    public NativeTickView(String world, boolean fluid, LongSupplier gameTime) {
        this.world = world;
        this.fluid = fluid;
        this.gameTime = gameTime;
    }

    @Override
    public void schedule(ScheduledTick<T> tick) {
        int delay = (int) Math.max(0L, tick.triggerTick() - gameTime.getAsLong());
        WorldNatives.scheduleTick(world, tick.pos().getX(), tick.pos().getY(), tick.pos().getZ(), delay, fluid);
    }

    @Override
    public boolean hasScheduledTick(BlockPos pos, T type) {
        return !fluid && WorldNatives.hasTick(world, pos.getX(), pos.getY(), pos.getZ());
    }

    @Override
    public int count() {
        return 0;
    }
}

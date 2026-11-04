// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;

import java.util.function.BiConsumer;
import java.util.function.LongPredicate;
import java.util.function.LongSupplier;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import org.basaltmc.bukkit.WorldNatives;

public class LevelTicks<T> implements LevelTickAccess<T> {
    private final String world;
    private final boolean fluid;
    private final LongSupplier gameTime;

    public LevelTicks(LongPredicate tickCheck) {
        this(null, false, () -> 0L);
    }

    public LevelTicks(String world, boolean fluid, LongSupplier gameTime) {
        this.world = world;
        this.fluid = fluid;
        this.gameTime = gameTime;
    }

    @Override
    public void schedule(ScheduledTick<T> tick) {
        if (world != null) {
            int delay = (int) Math.max(0L, tick.triggerTick() - gameTime.getAsLong());
            WorldNatives.scheduleTick(world, tick.pos().getX(), tick.pos().getY(), tick.pos().getZ(), delay, fluid);
        }
    }

    @Override
    public boolean hasScheduledTick(BlockPos pos, T type) {
        return world != null && !fluid && WorldNatives.hasTick(world, pos.getX(), pos.getY(), pos.getZ());
    }

    public boolean willTickThisTick(BlockPos pos, T type) {
        return false;
    }

    @Override
    public int count() {
        return 0;
    }

    public void addContainer(ChunkPos pos, LevelChunkTicks<T> container) {
    }

    public void removeContainer(ChunkPos pos) {
    }

    public void tick(long currentTick, int maxTicksToProcess, BiConsumer<BlockPos, T> output) {
    }

    public void clearArea(BoundingBox area) {
    }

    public void copyArea(BoundingBox area, Vec3i offset) {
    }

    public void copyAreaFrom(LevelTicks<T> source, BoundingBox area, Vec3i offset) {
    }

    interface PosAndContainerConsumer<T> {
        void accept(long pos, LevelChunkTicks<T> container);
    }
}

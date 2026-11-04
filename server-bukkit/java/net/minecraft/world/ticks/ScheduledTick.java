// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;

import it.unimi.dsi.fastutil.Hash;
import java.util.Comparator;
import net.minecraft.core.BlockPos;

public record ScheduledTick<T>(T type, BlockPos pos, long triggerTick, TickPriority priority, long subTickOrder) {
    public static final Comparator<ScheduledTick<?>> DRAIN_ORDER = Comparator
        .<ScheduledTick<?>>comparingLong(ScheduledTick::triggerTick)
        .thenComparing(ScheduledTick::priority)
        .thenComparingLong(ScheduledTick::subTickOrder);
    public static final Comparator<ScheduledTick<?>> INTRA_TICK_DRAIN_ORDER = Comparator
        .<ScheduledTick<?>, TickPriority>comparing(ScheduledTick::priority)
        .thenComparingLong(ScheduledTick::subTickOrder);
    public static final Hash.Strategy<ScheduledTick<?>> UNIQUE_TICK_HASH = new Hash.Strategy<>() {
        @Override
        public int hashCode(ScheduledTick<?> tick) {
            return 31 * tick.pos().hashCode() + tick.type().hashCode();
        }

        @Override
        public boolean equals(ScheduledTick<?> a, ScheduledTick<?> b) {
            return a == b || (a != null && b != null && a.type() == b.type() && a.pos().equals(b.pos()));
        }
    };

    public ScheduledTick(T type, BlockPos pos, long triggerTick, long subTickOrder) {
        this(type, pos, triggerTick, TickPriority.NORMAL, subTickOrder);
    }

    public static <T> ScheduledTick<T> probe(T type, BlockPos pos) {
        return new ScheduledTick<>(type, pos, 0L, TickPriority.NORMAL, 0L);
    }

    public SavedTick<T> toSavedTick(long currentTick) {
        return new SavedTick<>(type, pos, (int) (triggerTick - currentTick), priority);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.clock;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
public record ClockTimeMarker(Holder<WorldClock> clock, int ticks, Optional<Integer> periodTicks, boolean showInCommands) {
    public static final Codec<ResourceKey<ClockTimeMarker>> KEY_CODEC = ResourceKey.codec(ClockTimeMarkers.ROOT_ID);
    public long getRepetitionCount(long totalTicks) {
        if (periodTicks.isEmpty()) {
            return totalTicks >= ticks ? 1L : 0L;
        }
        long period = periodTicks.get();
        return totalTicks / period + (totalTicks % period >= ticks ? 1 : 0);
    }
    public long resolveTimeToMoveTo(long totalTicks) {
        if (periodTicks.isEmpty()) {
            return ticks;
        }
        int period = periodTicks.get();
        return totalTicks + untilNext(period, totalTicks % period, ticks);
    }
    public boolean occursAt(long totalTicks) {
        return periodTicks.isEmpty() ? ticks == totalTicks : ticks == totalTicks % periodTicks.get();
    }
    private static long untilNext(int period, long from, long to) {
        long gap = to - from;
        return gap > 0L ? gap : period + gap;
    }
}

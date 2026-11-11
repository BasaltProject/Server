// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;

import java.util.function.BooleanSupplier;
import java.util.function.IntSupplier;
import java.util.function.LongSupplier;

/** The in-game profiler is disabled: it never enables and always reports empty results. */
public class ContinuousProfiler {
    public ContinuousProfiler(LongSupplier realTime, IntSupplier tickCount, BooleanSupplier suppressWarnings) {
    }

    public boolean isEnabled() {
        return false;
    }

    public void disable() {
    }

    public void enable() {
    }

    public ProfilerFiller getFiller() {
        return InactiveProfiler.INSTANCE;
    }

    public ProfileResults getResults() {
        return EmptyProfileResults.EMPTY;
    }
}

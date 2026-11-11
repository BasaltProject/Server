// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;

import java.util.function.LongSupplier;

/** Slow-tick profiling is disabled: ticks are timed natively, so this hands out the inactive profiler. */
public class SingleTickProfiler {
    public SingleTickProfiler(LongSupplier realTime, String location, long saveThresholdNs) {
    }

    public ProfilerFiller startTick() {
        return InactiveProfiler.INSTANCE;
    }

    public void endTick() {
    }

    public static SingleTickProfiler createTickProfiler(String name) {
        return null;
    }

    public static ProfilerFiller decorateFiller(ProfilerFiller filler, SingleTickProfiler tickProfiler) {
        return filler;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics.profiling;

import java.util.HashSet;
import java.util.Set;
import java.util.function.LongSupplier;
import java.util.function.Supplier;
import net.minecraft.util.profiling.ProfileCollector;
import net.minecraft.util.profiling.metrics.MetricCategory;
import net.minecraft.util.profiling.metrics.MetricSampler;
import net.minecraft.util.profiling.metrics.MetricsSamplerProvider;

public class ServerMetricsSamplersProvider implements MetricsSamplerProvider {
    private final Set<MetricSampler> samplers = new HashSet<>();

    public ServerMetricsSamplersProvider(LongSupplier wallTimeSource, boolean isDedicatedServer) {
        samplers.addAll(runtimeIndependentSamplers());
        samplers.add(tickTimeSampler(wallTimeSource));
    }

    public static Set<MetricSampler> runtimeIndependentSamplers() {
        Runtime runtime = Runtime.getRuntime();
        Set<MetricSampler> result = new HashSet<>();
        result.add(MetricSampler.create("heap-used", MetricCategory.JVM, () -> runtime.totalMemory() - runtime.freeMemory()));
        return result;
    }

    @Override
    public Set<MetricSampler> samplers(Supplier<ProfileCollector> profiler) {
        return samplers;
    }

    public static MetricSampler tickTimeSampler(LongSupplier timeSource) {
        long[] last = {timeSource.getAsLong()};
        return MetricSampler.create("ticktime", MetricCategory.TICK_LOOP, () -> {
            long now = timeSource.getAsLong();
            long elapsed = now - last[0];
            last[0] = now;
            return elapsed / 1.0E6D;
        });
    }

    public static class CpuStats {
        public final int nrOfCpus = Runtime.getRuntime().availableProcessors();

        public double loadForCpu(int i) {
            return 0.0D;
        }
    }
}

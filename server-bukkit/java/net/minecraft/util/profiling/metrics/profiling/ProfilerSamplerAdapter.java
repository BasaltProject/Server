// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics.profiling;
import java.util.Set;
import java.util.function.Supplier;
import net.minecraft.util.profiling.ProfileCollector;
import net.minecraft.util.profiling.metrics.MetricSampler;
public class ProfilerSamplerAdapter {
    public Set<MetricSampler> newSamplersFoundInProfiler(Supplier<ProfileCollector> profiler) {
        return java.util.Set.of();
    }
}

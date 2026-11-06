// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics;

import java.util.List;
public interface ProfilerMeasured {
    List<MetricSampler> profiledMetrics();
}

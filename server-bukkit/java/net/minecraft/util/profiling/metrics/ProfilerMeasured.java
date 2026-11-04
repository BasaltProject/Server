// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics;

import java.util.List;
public interface ProfilerMeasured {
    default List<MetricSampler> profiledMetrics() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.profiling.metrics.ProfilerMeasured#profiledMetrics");
    }
}

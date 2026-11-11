// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.WeakHashMap;

public class MetricsRegistry {
    public static final MetricsRegistry INSTANCE = new MetricsRegistry();
    private final WeakHashMap<ProfilerMeasured, Void> measured = new WeakHashMap<>();

    private MetricsRegistry() {
    }

    public synchronized void add(ProfilerMeasured profilerMeasured) {
        measured.put(profilerMeasured, null);
    }

    public synchronized List<MetricSampler> getRegisteredSamplers() {
        Map<String, List<MetricSampler>> byName = new LinkedHashMap<>();
        for (ProfilerMeasured source : measured.keySet()) {
            for (MetricSampler sampler : source.profiledMetrics()) {
                byName.computeIfAbsent(sampler.getName(), key -> new ArrayList<>()).add(sampler);
            }
        }
        List<MetricSampler> result = new ArrayList<>();
        byName.forEach((name, group) -> result.add(group.size() > 1 ? new AggregatedMetricSampler(name, group) : group.get(0)));
        return result;
    }

    static class AggregatedMetricSampler extends MetricSampler {
        private final List<MetricSampler> delegates;

        private AggregatedMetricSampler(String name, List<MetricSampler> delegates) {
            super(name, SamplingPhase.END_TICK, delegates.get(0).getCategory(), () -> average(delegates), () -> delegates.forEach(MetricSampler::onStartTick),
                value -> delegates.stream().anyMatch(d -> d.thresholdTest != null && d.thresholdTest.test(value)));
            this.delegates = delegates;
        }

        private static double average(List<MetricSampler> delegates) {
            double sum = 0.0D;
            for (MetricSampler delegate : delegates) {
                sum += delegate.getSampler().getAsDouble();
            }
            return sum / delegates.size();
        }

        @Override
        public boolean equals(Object o) {
            return this == o || o != null && getClass() == o.getClass() && super.equals(o) && delegates.equals(((AggregatedMetricSampler) o).delegates);
        }

        @Override
        public int hashCode() {
            return java.util.Objects.hash(super.hashCode(), delegates);
        }
    }
}

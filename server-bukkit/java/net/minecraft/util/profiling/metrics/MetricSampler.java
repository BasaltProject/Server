// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics;

import it.unimi.dsi.fastutil.doubles.DoubleArrayList;
import it.unimi.dsi.fastutil.ints.Int2DoubleMap;
import it.unimi.dsi.fastutil.ints.Int2DoubleOpenHashMap;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import java.util.Locale;
import java.util.function.Consumer;
import java.util.function.DoubleSupplier;
import java.util.function.ToDoubleFunction;

public class MetricSampler {
    private final String name;
    private final SamplingPhase samplingPhase;
    private final MetricCategory category;
    private final DoubleSupplier sampler;
    private final IntArrayList ticks = new IntArrayList();
    private final DoubleArrayList values = new DoubleArrayList();
    private volatile boolean running = true;
    private final Runnable beforeTick;
    final ThresholdTest thresholdTest;
    private double currentValue;

    protected MetricSampler(String name, SamplingPhase samplingPhase, MetricCategory category, DoubleSupplier sampler, Runnable beforeTick,
            ThresholdTest thresholdTest) {
        this.name = name;
        this.samplingPhase = samplingPhase;
        this.category = category;
        this.sampler = sampler;
        this.beforeTick = beforeTick;
        this.thresholdTest = thresholdTest;
    }

    public static MetricSampler create(String name, MetricCategory category, DoubleSupplier sampler) {
        return new MetricSampler(name, SamplingPhase.END_TICK, category, sampler, null, null);
    }

    public static MetricSampler createExtractSampler(String name, MetricCategory category, DoubleSupplier sampler) {
        return new MetricSampler(name, SamplingPhase.EXTRACT, category, sampler, null, null);
    }

    public static <T> MetricSamplerBuilder<T> builder(String metricName, MetricCategory category, ToDoubleFunction<T> sampler, T context) {
        if (sampler == null) {
            throw new IllegalStateException();
        }
        return new MetricSamplerBuilder<>(metricName, category, sampler, context);
    }

    public void onStartTick() {
        if (!running) {
            throw new IllegalStateException("Not running");
        }
        if (beforeTick != null) {
            beforeTick.run();
        }
    }

    public void onEndTick(int currentTick) {
        requireRunning();
        currentValue = sampler.getAsDouble();
        values.add(currentValue);
        ticks.add(currentTick);
    }

    public void onFinished() {
        requireRunning();
        running = false;
    }

    private void requireRunning() {
        if (!running) {
            throw new IllegalStateException(String.format(Locale.ROOT, "Sampler for metric %s not started!", name));
        }
    }

    public DoubleSupplier getSampler() {
        return sampler;
    }

    public String getName() {
        return name;
    }

    public SamplingPhase samplingPhase() {
        return samplingPhase;
    }

    public MetricCategory getCategory() {
        return category;
    }

    public SamplerResult result() {
        Int2DoubleMap recording = new Int2DoubleOpenHashMap();
        int first = Integer.MIN_VALUE;
        int last = Integer.MIN_VALUE;
        for (int i = 0; i < values.size(); i++) {
            int tick = ticks.getInt(i);
            if (first == Integer.MIN_VALUE) {
                first = tick;
            }
            recording.put(tick, values.getDouble(i));
            last = tick;
        }
        ticks.clear();
        values.clear();
        return new SamplerResult(first, last, recording);
    }

    public boolean triggersThreshold() {
        return thresholdTest != null && thresholdTest.test(currentValue);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        MetricSampler that = (MetricSampler) o;
        return name.equals(that.name) && category.equals(that.category);
    }

    @Override
    public int hashCode() {
        return name.hashCode();
    }

    public static class SamplerResult {
        private final Int2DoubleMap recording;
        private final int firstTick;
        private final int lastTick;

        public SamplerResult(int firstTick, int lastTick, Int2DoubleMap recording) {
            this.firstTick = firstTick;
            this.lastTick = lastTick;
            this.recording = recording;
        }

        public double valueAtTick(int tick) {
            return recording.get(tick);
        }

        public int getFirstTick() {
            return firstTick;
        }

        public int getLastTick() {
            return lastTick;
        }
    }

    public static class ValueIncreasedByPercentage implements ThresholdTest {
        private final float threshold;
        private double previous = Double.MIN_VALUE;

        public ValueIncreasedByPercentage(float percentageIncreaseThreshold) {
            this.threshold = percentageIncreaseThreshold;
        }

        @Override
        public boolean test(double value) {
            boolean exceeded = previous != Double.MIN_VALUE && value > previous && (value - previous) / previous >= threshold;
            previous = value;
            return exceeded;
        }
    }

    public static class MetricSamplerBuilder<T> {
        private final String name;
        private final MetricCategory category;
        private final DoubleSupplier sampler;
        private final T context;
        private SamplingPhase samplingPhase = SamplingPhase.END_TICK;
        private Runnable beforeTick;
        private ThresholdTest thresholdTest;

        public MetricSamplerBuilder(String name, MetricCategory category, ToDoubleFunction<T> sampler, T context) {
            this.name = name;
            this.category = category;
            this.sampler = () -> sampler.applyAsDouble(context);
            this.context = context;
        }

        public MetricSamplerBuilder<T> withBeforeTick(Consumer<T> beforeTick) {
            this.beforeTick = () -> beforeTick.accept(context);
            return this;
        }

        public MetricSamplerBuilder<T> withThresholdAlert(ThresholdTest thresholdTest) {
            this.thresholdTest = thresholdTest;
            return this;
        }

        public MetricSamplerBuilder<T> withSamplingPhase(SamplingPhase samplingPhase) {
            this.samplingPhase = samplingPhase;
            return this;
        }

        public MetricSampler build() {
            return new MetricSampler(name, samplingPhase, category, sampler, beforeTick, thresholdTest);
        }
    }

    public static enum SamplingPhase {
        EXTRACT, END_TICK;
    }

    public interface ThresholdTest {
        boolean test(double value);
    }
}

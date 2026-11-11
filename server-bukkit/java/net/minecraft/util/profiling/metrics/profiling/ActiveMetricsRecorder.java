// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics.profiling;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executor;
import java.util.function.Consumer;
import java.util.function.LongSupplier;
import net.minecraft.util.profiling.EmptyProfileResults;
import net.minecraft.util.profiling.InactiveProfiler;
import net.minecraft.util.profiling.ProfileResults;
import net.minecraft.util.profiling.ProfilerFiller;
import net.minecraft.util.profiling.metrics.MetricSampler;
import net.minecraft.util.profiling.metrics.MetricsSamplerProvider;
import net.minecraft.util.profiling.metrics.storage.MetricsPersister;

public class ActiveMetricsRecorder implements MetricsRecorder {
    public static final int PROFILING_MAX_DURATION_SECONDS = 10;
    private static final List<Consumer<Path>> kGlobalCallbacks = new CopyOnWriteArrayList<>();
    private final List<MetricSampler> samplers;
    private final Consumer<ProfileResults> onProfilingEnd;
    private final LongSupplier timeSource;
    private final long startedAt;
    private final int startTick;
    private int currentTick;
    private boolean recording = true;

    private ActiveMetricsRecorder(MetricsSamplerProvider provider, LongSupplier timeSource, Consumer<ProfileResults> onProfilingEnd) {
        this.samplers = new ArrayList<>(provider.samplers(() -> null));
        this.timeSource = timeSource;
        this.onProfilingEnd = onProfilingEnd;
        this.startedAt = timeSource.getAsLong();
        this.startTick = 0;
    }

    public static ActiveMetricsRecorder createStarted(MetricsSamplerProvider metricsSamplerProvider, LongSupplier timeSource, Executor ioExecutor,
            MetricsPersister metricsPersister, Consumer<ProfileResults> onProfilingEnd, Consumer<Path> onReportFinished) {
        return new ActiveMetricsRecorder(metricsSamplerProvider, timeSource, onProfilingEnd);
    }

    @Override
    public void end() {
        if (!recording) {
            return;
        }
        recording = false;
        samplers.forEach(MetricSampler::onFinished);
        onProfilingEnd.accept(EmptyProfileResults.EMPTY);
    }

    @Override
    public void cancel() {
        if (!recording) {
            return;
        }
        recording = false;
        samplers.forEach(MetricSampler::onFinished);
    }

    @Override
    public void startTick() {
        if (recording) {
            samplers.forEach(MetricSampler::onStartTick);
        }
    }

    @Override
    public void sampleDuringExtract() {
        if (recording) {
            currentTick++;
        }
    }

    @Override
    public void endTick() {
        if (!recording) {
            return;
        }
        int tick = startTick + currentTick++;
        samplers.forEach(sampler -> sampler.onEndTick(tick));
        if (timeSource.getAsLong() - startedAt > PROFILING_MAX_DURATION_SECONDS * 1_000_000_000L) {
            end();
        }
    }

    @Override
    public boolean isRecording() {
        return recording;
    }

    @Override
    public ProfilerFiller getProfiler() {
        return InactiveProfiler.INSTANCE;
    }

    public static void registerGlobalCompletionCallback(Consumer<Path> onFinished) {
        kGlobalCallbacks.add(onFinished);
    }
}

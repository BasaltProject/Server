// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import com.mojang.datafixers.util.Pair;
import it.unimi.dsi.fastutil.objects.Object2LongMap;
import it.unimi.dsi.fastutil.objects.Object2LongOpenHashMap;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.function.IntSupplier;
import java.util.function.LongSupplier;
import java.util.function.Supplier;
import net.minecraft.util.profiling.metrics.MetricCategory;
public class ActiveProfiler implements ProfileCollector {
    private static final long kWarnThresholdNanos = 100L;
    private final List<String> paths = new ArrayList<>();
    private final List<Long> startTimes = new ArrayList<>();
    private final Map<String, PathEntry> entries = new HashMap<>();
    private final LongSupplier getRealTime;
    private final IntSupplier getTickTime;
    private final long startTimeNano;
    private final int startTimeTicks;
    private final BooleanSupplier suppressWarnings;
    private final Set<Pair<String, MetricCategory>> chartedPaths = new HashSet<>();
    private String path = "";
    private boolean started;
    private PathEntry currentEntry;
    public ActiveProfiler(LongSupplier getRealTime, IntSupplier getTickTime, BooleanSupplier suppressWarnings) {
        this.getRealTime = getRealTime;
        this.getTickTime = getTickTime;
        this.startTimeNano = getRealTime.getAsLong();
        this.startTimeTicks = getTickTime.getAsInt();
        this.suppressWarnings = suppressWarnings;
    }
    @Override
    public void startTick() {
        if (started) {
            throw new IllegalStateException("Profiler already started");
        }
        started = true;
        path = "";
        paths.clear();
        push(ProfilerFiller.ROOT);
    }
    @Override
    public void endTick() {
        if (!started) {
            throw new IllegalStateException("Profiler not started");
        }
        pop();
        started = false;
    }
    @Override
    public void push(String name) {
        if (!started) {
            return;
        }
        if (!path.isEmpty()) {
            path += ProfileResults.PATH_SEPARATOR;
        }
        path += name;
        paths.add(path);
        startTimes.add(System.nanoTime());
        currentEntry = null;
    }
    @Override
    public void push(Supplier<String> name) {
        push(name.get());
    }
    @Override
    public void markForCharting(MetricCategory category) {
        chartedPaths.add(Pair.of(path, category));
    }
    @Override
    public void pop() {
        if (!started || startTimes.isEmpty()) {
            return;
        }
        long elapsed = System.nanoTime() - startTimes.remove(startTimes.size() - 1);
        paths.remove(paths.size() - 1);
        PathEntry entry = entryFor(path);
        entry.accumulatedDuration += elapsed;
        entry.count++;
        entry.maxDuration = Math.max(entry.maxDuration, elapsed);
        entry.minDuration = Math.min(entry.minDuration, elapsed);
        path = paths.isEmpty() ? "" : paths.get(paths.size() - 1);
        currentEntry = null;
    }
    @Override
    public void popPush(String name) {
        pop();
        push(name);
    }
    @Override
    public void popPush(Supplier<String> name) {
        pop();
        push(name);
    }
    private PathEntry entryFor(String key) {
        return entries.computeIfAbsent(key, ignored -> new PathEntry());
    }
    @Override
    public void incrementCounter(String name, int amount) {
        if (started) {
            if (currentEntry == null) {
                currentEntry = entryFor(path);
            }
            currentEntry.counters.addTo(name, amount);
        }
    }
    @Override
    public void incrementCounter(Supplier<String> name, int amount) {
        if (started) {
            incrementCounter(name.get(), amount);
        }
    }
    @Override
    public ProfileResults getResults() {
        return new FilledProfileResults(entries, startTimeNano, startTimeTicks, getRealTime.getAsLong(), getTickTime.getAsInt());
    }
    @Override
    public PathEntry getEntry(String path) {
        return entries.get(path);
    }
    @Override
    public Set<Pair<String, MetricCategory>> getChartedPaths() {
        return chartedPaths;
    }
    public static class PathEntry implements ProfilerPathEntry {
        private long maxDuration;
        private long minDuration = Long.MAX_VALUE;
        private long accumulatedDuration;
        private long count;
        private final Object2LongOpenHashMap<String> counters = new Object2LongOpenHashMap<>();
        @Override
        public long getDuration() {
            return accumulatedDuration;
        }
        @Override
        public long getMaxDuration() {
            return maxDuration;
        }
        @Override
        public long getCount() {
            return count;
        }
        @Override
        public Object2LongMap<String> getCounters() {
            return counters;
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import jdk.jfr.consumer.RecordedEvent;
public record GcHeapStat(Instant timestamp, long heapUsed, Timing timing) {
    public static GcHeapStat from(RecordedEvent event) {
        Timing when = event.getString("when").equalsIgnoreCase("before gc") ? Timing.BEFORE_GC : Timing.AFTER_GC;
        return new GcHeapStat(event.getStartTime(), event.getLong("heapUsed"), when);
    }
    public static Summary summary(Duration recordingDuration, List<GcHeapStat> heapStats, Duration gcTotalDuration, int totalGCs) {
        return new Summary(recordingDuration, gcTotalDuration, totalGCs, allocationRate(heapStats));
    }
    private static double allocationRate(List<GcHeapStat> heapStats) {
        Map<Timing, List<GcHeapStat>> byTiming = heapStats.stream().collect(Collectors.groupingBy(stat -> stat.timing));
        List<GcHeapStat> before = byTiming.get(Timing.BEFORE_GC);
        List<GcHeapStat> after = byTiming.get(Timing.AFTER_GC);
        long allocated = 0L;
        for (int i = 1; i < before.size(); i++) {
            allocated += before.get(i).heapUsed - after.get(i - 1).heapUsed;
        }
        Duration span = Duration.between(heapStats.get(1).timestamp, heapStats.get(heapStats.size() - 1).timestamp);
        return (double) allocated / span.getSeconds();
    }
    public static record Summary(Duration duration, Duration gcTotalDuration, int totalGCs, double allocationRateBytesPerSecond) {
        public float gcOverHead() {
            return (float) gcTotalDuration.toMillis() / (float) duration.toMillis();
        }
    }
    public static enum Timing {
        BEFORE_GC,
        AFTER_GC
    }
}

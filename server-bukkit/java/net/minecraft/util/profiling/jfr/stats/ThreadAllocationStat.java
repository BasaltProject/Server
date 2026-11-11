// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import com.google.common.base.MoreObjects;
import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.stream.Collectors;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordedThread;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public record ThreadAllocationStat(Instant timestamp, String threadName, long totalBytes) {
    public static final Logger LOGGER = LoggerFactory.getLogger(ThreadAllocationStat.class);
    private static final String kUnknownThread = "unknown";
    public static ThreadAllocationStat from(RecordedEvent event) {
        RecordedThread thread = event.getThread("thread");
        String name = thread == null ? kUnknownThread : MoreObjects.firstNonNull(thread.getJavaName(), kUnknownThread);
        return new ThreadAllocationStat(event.getStartTime(), name, event.getLong("allocated"));
    }
    public static Summary summary(List<ThreadAllocationStat> stats) {
        Map<String, Double> perSecond = new TreeMap<>();
        stats.stream().collect(Collectors.groupingBy(stat -> stat.threadName)).forEach((thread, samples) -> {
            if (samples.size() < 2) {
                return;
            }
            ThreadAllocationStat first = samples.getFirst();
            ThreadAllocationStat last = samples.getLast();
            long seconds = Duration.between(first.timestamp, last.timestamp).getSeconds();
            if (seconds <= 0L) {
                LOGGER.warn("Thread allocation stat timestamps are not in chronological order for thread {}, skipping it", thread);
            } else {
                perSecond.put(thread, (double) (last.totalBytes - first.totalBytes) / seconds);
            }
        });
        return new Summary(perSecond);
    }
    public static record Summary(Map<String, Double> allocationsPerSecondByThread) {
    }
}

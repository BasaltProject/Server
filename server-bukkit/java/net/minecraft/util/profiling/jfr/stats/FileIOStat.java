// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import com.mojang.datafixers.util.Pair;
import java.time.Duration;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
public record FileIOStat(Duration duration, String path, long bytes) {
    public static Summary summary(Duration recordingDuration, List<FileIOStat> ioStats) {
        long total = ioStats.stream().mapToLong(stat -> stat.bytes).sum();
        double seconds = recordingDuration.getSeconds();
        Duration spent = ioStats.stream().map(FileIOStat::duration).reduce(Duration.ZERO, Duration::plus);
        Map<String, Long> bytesByPath = ioStats.stream().filter(stat -> stat.path != null)
                .collect(Collectors.groupingBy(stat -> stat.path, Collectors.summingLong(stat -> stat.bytes)));
        List<Pair<String, Long>> top = bytesByPath.entrySet().stream()
                .sorted(Map.Entry.<String, Long>comparingByValue().reversed())
                .map(e -> Pair.of(e.getKey(), e.getValue())).limit(10L).toList();
        return new Summary(total, total / seconds, ioStats.size(), ioStats.size() / seconds, spent, top);
    }
    public static record Summary(long totalBytes, double bytesPerSecond, long counts, double countsPerSecond, Duration timeSpentInIO,
            List<Pair<String, Long>> topTenContributorsByTotalBytes) {
    }
}

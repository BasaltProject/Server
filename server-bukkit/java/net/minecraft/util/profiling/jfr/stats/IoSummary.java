// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import com.mojang.datafixers.util.Pair;
import java.time.Duration;
import java.util.Comparator;
import java.util.List;
public final class IoSummary<T> {
    private final CountAndSize total;
    private final List<Pair<T, CountAndSize>> largest;
    private final Duration duration;
    public IoSummary(Duration recordingDuration, List<Pair<T, CountAndSize>> packetStats) {
        this.duration = recordingDuration;
        this.total = packetStats.stream().map(Pair::getSecond).reduce(new CountAndSize(0L, 0L), CountAndSize::add);
        this.largest = packetStats.stream().sorted(Comparator.comparing(Pair::getSecond, CountAndSize.kBySize)).limit(10L).toList();
    }
    public double getCountsPerSecond() {
        return (double) total.totalCount / duration.getSeconds();
    }
    public double getSizePerSecond() {
        return (double) total.totalSize / duration.getSeconds();
    }
    public long getTotalCount() {
        return total.totalCount;
    }
    public long getTotalSize() {
        return total.totalSize;
    }
    public List<Pair<T, CountAndSize>> largestSizeContributors() {
        return largest;
    }
    public static record CountAndSize(long totalCount, long totalSize) {
        private static final Comparator<CountAndSize> kBySize = Comparator.comparing(CountAndSize::totalSize).thenComparing(CountAndSize::totalCount).reversed();
        public CountAndSize add(CountAndSize that) {
            return new CountAndSize(totalCount + that.totalCount, totalSize + that.totalSize);
        }
        public float averageSize() {
            return (float) totalSize / (float) totalCount;
        }
    }
}

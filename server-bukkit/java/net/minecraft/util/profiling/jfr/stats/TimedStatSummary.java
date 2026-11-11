// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import java.time.Duration;
import java.util.List;
import java.util.Map;
import java.util.Optional;
public record TimedStatSummary<T extends TimedStat>(T fastest, T slowest, T secondSlowest, int count, Map<Integer, Double> percentilesNanos, Duration totalDuration) {
    public static <T extends TimedStat> Optional<TimedStatSummary<T>> summary(List<T> values) {
        return java.util.Optional.empty();
    }
}

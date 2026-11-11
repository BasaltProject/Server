// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import java.util.Optional;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import org.apache.commons.lang3.mutable.MutableInt;
public class FeatureCountTracker {
    private static final java.util.Map<ServerLevel, LevelData> kData = java.util.Collections.synchronizedMap(new java.util.WeakHashMap<>());
    private static LevelData of(ServerLevel level) {
        return kData.computeIfAbsent(level, l -> new LevelData(new java.util.concurrent.ConcurrentHashMap<>(), new java.util.concurrent.atomic.AtomicInteger()));
    }
    public static void chunkDecorated(ServerLevel level) {
        of(level).chunksWithFeatures().incrementAndGet();
    }
    public static void featurePlaced(ServerLevel level, Feature feature, Optional<PlacedFeature> topFeature) {
        of(level).featureData().merge(new FeatureData(feature, topFeature), 1, Integer::sum);
    }
    public static void clearCounts() {
        kData.clear();
    }
    public static void logCounts() {
        org.slf4j.Logger log = org.slf4j.LoggerFactory.getLogger(FeatureCountTracker.class);
        log.debug("Logging feature counts:");
        synchronized (kData) {
            kData.forEach((level, counts) -> {
                String prefix = (level.getServer().isRunning() ? "running" : "dead") + " " + level.dimension().identifier();
                int chunks = counts.chunksWithFeatures().get();
                log.debug("{} total_chunks: {}", prefix, chunks);
                counts.featureData().forEach((entry, count) -> log.debug("{} {} {}", prefix,
                    String.format(java.util.Locale.ROOT, "%10d", count), String.format(java.util.Locale.ROOT, "%10f", (double) count / (double) chunks)));
            });
        }


    }
    static record FeatureData(Feature feature, Optional<PlacedFeature> topFeature) {
    }
    static record LevelData(java.util.Map<FeatureData, Integer> featureData, java.util.concurrent.atomic.AtomicInteger chunksWithFeatures) {
    }
}

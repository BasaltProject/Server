// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.metrics.storage;

import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.minecraft.util.profiling.ProfileResults;
import net.minecraft.util.profiling.metrics.MetricSampler;

public class MetricsPersister {
    public static final Path PROFILING_RESULTS_DIR = Path.of("debug/profiling");
    public static final String METRICS_DIR_NAME = "metrics";
    public static final String DEVIATIONS_DIR_NAME = "deviations";
    public static final String PROFILING_RESULT_FILENAME = "profiling.txt";
    private final String rootFolderName;

    public MetricsPersister(String rootFolderName) {
        this.rootFolderName = rootFolderName;
    }

    public Path saveReports(Set<MetricSampler> samplers, Map<MetricSampler, List<RecordedDeviation>> deviationsBySampler, ProfileResults profilerResults) {
        Path root = PROFILING_RESULTS_DIR.resolve(rootFolderName);
        try {
            Files.createDirectories(root);
        } catch (java.io.IOException e) {
            throw new UncheckedIOException(e);
        }
        profilerResults.saveResults(root.resolve(PROFILING_RESULT_FILENAME));
        return root;
    }
}

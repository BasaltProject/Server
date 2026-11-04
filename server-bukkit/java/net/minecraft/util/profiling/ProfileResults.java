// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import java.nio.file.Path;
import java.util.List;
public interface ProfileResults {
    char PATH_SEPARATOR = '\u001e';
    List<ResultField> getTimes(String path);
    boolean saveResults(Path file);
    long getStartTimeNano();
    int getStartTimeTicks();
    long getEndTimeNano();
    int getEndTimeTicks();
    default long getNanoDuration() {
        return getEndTimeNano() - getStartTimeNano();
    }
    default int getTickDuration() {
        return getEndTimeTicks() - getStartTimeTicks();
    }
    String getProfilerResults();
    static String demanglePath(String path) {
        return path.replace(PATH_SEPARATOR, '.');
    }
}

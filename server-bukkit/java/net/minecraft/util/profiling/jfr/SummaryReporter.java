// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr;
import java.nio.file.Path;
public class SummaryReporter {
    protected SummaryReporter(Runnable onDeregistration) {
        
    }
    public void recordingStopped(Path result) {
    }
}

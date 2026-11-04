// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import java.util.function.Supplier;
public class Zone implements AutoCloseable {
    public static final Zone INACTIVE = new Zone(null);
    private final ProfilerFiller profiler;
    Zone(ProfilerFiller profiler) {
        this.profiler = profiler;
    }
    public Zone addText(String text) {
        if (profiler != null) {
            profiler.addZoneText(text);
        }
        return this;
    }
    public Zone addText(Supplier<String> text) {
        return profiler == null ? this : addText(text.get());
    }
    public Zone addValue(long value) {
        if (profiler != null) {
            profiler.addZoneValue(value);
        }
        return this;
    }
    public Zone setColor(int color) {
        if (profiler != null) {
            profiler.setZoneColor(color);
        }
        return this;
    }
    public void close() {
        if (profiler != null) {
            profiler.pop();
        }
    }
}

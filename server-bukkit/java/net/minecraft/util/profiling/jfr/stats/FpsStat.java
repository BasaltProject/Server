// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import jdk.jfr.consumer.RecordedEvent;
public record FpsStat(int fps) {
    public static FpsStat from(RecordedEvent event, String field) {
        return new FpsStat(event.getInt(field));
    }
}

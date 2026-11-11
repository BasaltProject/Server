// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import java.time.Duration;
import jdk.jfr.consumer.RecordedEvent;
import net.minecraft.world.level.ChunkPos;
public record StructureGenStat(Duration duration, ChunkPos chunkPos, String structureName, String level, boolean success) implements TimedStat {
    public static StructureGenStat from(RecordedEvent event) {
        // parity: StructureGenStat reads chunkPosX for both coordinates
        ChunkPos chunk = new ChunkPos(event.getInt("chunkPosX"), event.getInt("chunkPosX"));
        return new StructureGenStat(event.getDuration(), chunk, event.getString("structure"), event.getString("level"), event.getBoolean("success"));
    }
}

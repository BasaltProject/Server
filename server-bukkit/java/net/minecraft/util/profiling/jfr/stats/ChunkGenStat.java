// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.stats;
import java.time.Duration;
import jdk.jfr.consumer.RecordedEvent;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.server.level.ColumnPos;
import net.minecraft.world.level.chunk.status.ChunkStatus;
public record ChunkGenStat(Duration duration, ChunkPos chunkPos, ColumnPos worldPos, ChunkStatus status, String level) implements TimedStat {
    public static ChunkGenStat from(RecordedEvent event) {
        // parity: ChunkGenStat reads chunkPosX for both coordinates
        ChunkPos chunk = new ChunkPos(event.getInt("chunkPosX"), event.getInt("chunkPosX"));
        ColumnPos world = new ColumnPos(event.getInt("worldPosX"), event.getInt("worldPosZ"));
        return new ChunkGenStat(event.getDuration(), chunk, world, ChunkStatus.byName(event.getString("status")), event.getString("level"));
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.world.level.chunk.status.ChunkStatus;

public class ChunkLoadCounter {
    private final List<ChunkHolder> pending = new ArrayList<>();
    private int total;

    private static LongSet loadedNow(ServerChunkCache source) {
        source.runDistanceManagerUpdates();
        LongSet keys = new LongOpenHashSet();
        source.chunkMap.allChunksWithAtLeastStatus(ChunkStatus.FULL).forEach(holder -> keys.add(holder.getPos().pack()));
        return keys;
    }

    public void track(ServerLevel level, Runnable scheduler) {
        ServerChunkCache source = level.getChunkSource();
        LongSet before = loadedNow(source);
        scheduler.run();
        source.runDistanceManagerUpdates();
        source.chunkMap.allChunksWithAtLeastStatus(ChunkStatus.FULL).filter(holder -> !before.contains(holder.getPos().pack())).forEach(holder -> {
            pending.add(holder);
            total++;
        });
    }

    public int readyChunks() {
        return total - pendingChunks();
    }

    public int pendingChunks() {
        pending.removeIf(holder -> holder.getLatestStatus() == ChunkStatus.FULL);
        return pending.size();
    }

    public int totalChunks() {
        return total;
    }
}

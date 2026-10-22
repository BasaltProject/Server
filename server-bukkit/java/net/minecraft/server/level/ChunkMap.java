package net.minecraft.server.level;

import java.util.List;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.storage.SimpleRegionStorage;
import org.basaltmc.bukkit.facade.ChunkResender;

public class ChunkMap extends SimpleRegionStorage implements ChunkHolder.PlayerProvider, GeneratingChunkMap {
    private final ServerChunkCache source;
    private final ServerPlayer watcher;

    protected ChunkMap() {
        this.source = null;
        this.watcher = null;
    }

    ChunkMap(ServerChunkCache source, String world) {
        this.source = source;
        this.watcher = new ServerPlayer(new ChunkResender(world, source));
    }

    /** Looked up by name by the world editing adapters. */
    ChunkHolder getVisibleChunkIfPresent(long packed) {
        LevelChunk chunk = source.getChunk(ChunkPos.unpack(packed).x(), ChunkPos.unpack(packed).z(), false);
        return chunk == null ? null : new ChunkHolder(chunk);
    }

    public List<ServerPlayer> getPlayers(ChunkPos pos, boolean borderOnly) {
        return List.of(watcher);
    }

    public void resendBiomesForChunks(List chunks) {
        for (Object chunk : chunks) {
            if (chunk instanceof LevelChunk levelChunk) {
                source.resend(levelChunk.getPos().x(), levelChunk.getPos().z());
            }
        }
    }
}

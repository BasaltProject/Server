package net.minecraft.server.level;

import java.util.List;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.storage.SimpleRegionStorage;
import org.basaltmc.bukkit.Unsupported;
public class ChunkMap extends SimpleRegionStorage implements ChunkHolder.PlayerProvider, GeneratingChunkMap {
    protected ChunkMap() {
    }
    public List getPlayers(ChunkPos pos, boolean borderOnly) {
        throw Unsupported.hit("net.minecraft.server.level.ChunkMap.getPlayers");
    }
    public void resendBiomesForChunks(List chunks) {
        throw Unsupported.hit("net.minecraft.server.level.ChunkMap.resendBiomesForChunks");
    }
}

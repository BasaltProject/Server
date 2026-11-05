// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.world.level.lighting.LevelLightEngine;
import org.basaltmc.bukkit.Unsupported;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.io.IOException;
import java.util.function.BooleanSupplier;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.status.ChunkStatus;
public abstract class ChunkSource implements LightChunkGetter, AutoCloseable {
    protected ChunkSource() {
    }
    public LevelLightEngine getLightEngine() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkSource.getLightEngine");
    }
    public abstract LevelChunk getChunk(int x, int z, boolean loadOrGenerate);
    public abstract LevelChunk getChunkNow(int x, int z);
    public abstract LightChunk getChunkForLighting(int x, int z);
    public abstract boolean hasChunk(int x, int z);
    public abstract ChunkAccess getChunk(int x, int z, ChunkStatus targetStatus, boolean loadOrGenerate);
    public abstract void tick(BooleanSupplier haveTime, boolean tickChunks);
    public abstract void onSectionEmptinessChanged(int sectionX, int sectionY, int sectionZ, boolean empty);
    public abstract String gatherStats();
    public abstract int getLoadedChunksCount();
    public abstract void close() throws IOException;
    public abstract void setSpawnSettings(boolean spawnEnemies);
    public abstract boolean updateChunkForced(ChunkPos pos, boolean forced);
    public abstract LongSet getForceLoadedChunks();
}

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
    public LevelChunk getChunk(int x, int z, boolean loadOrGenerate) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#getChunk");
    }
    public LevelChunk getChunkNow(int x, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#getChunkNow");
    }
    public LightChunk getChunkForLighting(int x, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#getChunkForLighting");
    }
    public boolean hasChunk(int x, int z) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#hasChunk");
    }
    public ChunkAccess getChunk(int x, int z, ChunkStatus targetStatus, boolean loadOrGenerate) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#getChunk");
    }
    public void tick(BooleanSupplier haveTime, boolean tickChunks) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#tick");
    }
    public void onSectionEmptinessChanged(int sectionX, int sectionY, int sectionZ, boolean empty) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#onSectionEmptinessChanged");
    }
    public String gatherStats() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#gatherStats");
    }
    public int getLoadedChunksCount() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#getLoadedChunksCount");
    }
    public void close() throws IOException {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#close");
    }
    public void setSpawnSettings(boolean spawnEnemies) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#setSpawnSettings");
    }
    public boolean updateChunkForced(ChunkPos pos, boolean forced) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#updateChunkForced");
    }
    public LongSet getForceLoadedChunks() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.ChunkSource#getForceLoadedChunks");
    }
}

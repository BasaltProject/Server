// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.concurrent.CompletableFuture;
import net.minecraft.util.StaticCache2D;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.chunk.status.ChunkStep;
public interface GeneratingChunkMap {
    default GenerationChunkHolder acquireGeneration(long chunkNode) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.GeneratingChunkMap#acquireGeneration");
    }
    default void releaseGeneration(GenerationChunkHolder chunkHolder) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.GeneratingChunkMap#releaseGeneration");
    }
    default CompletableFuture<ChunkAccess> applyStep(GenerationChunkHolder chunkHolder, ChunkStep step, StaticCache2D<GenerationChunkHolder> cache) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.GeneratingChunkMap#applyStep");
    }
    default ChunkGenerationTask scheduleGenerationTask(ChunkStatus targetStatus, ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.GeneratingChunkMap#scheduleGenerationTask");
    }
    default void runGenerationTasks() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.GeneratingChunkMap#runGenerationTasks");
    }
}

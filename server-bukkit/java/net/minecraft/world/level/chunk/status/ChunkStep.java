// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.status;

import java.util.concurrent.CompletableFuture;
import net.minecraft.server.level.GenerationChunkHolder;
import net.minecraft.util.StaticCache2D;
import net.minecraft.world.level.chunk.ChunkAccess;

public record ChunkStep(ChunkStatus targetStatus, Object directDependencies, Object accumulatedDependencies, int blockStateWriteRadius, Object task) {
    public int getAccumulatedRadiusOf(ChunkStatus status) {
        return 1;
    }
    public CompletableFuture<ChunkAccess> apply(WorldGenContext context, StaticCache2D<GenerationChunkHolder> cache, ChunkAccess chunk) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep#apply");
    }
    public static class Builder {
        protected Builder(ChunkStatus status) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep.Builder#<init>");
        }
        protected Builder(ChunkStatus status, ChunkStep parent) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep.Builder#<init>");
        }
        public Builder addRequirement(ChunkStatus status, int radius) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep.Builder#addRequirement");
        }
        public Builder blockStateWriteRadius(int radius) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep.Builder#blockStateWriteRadius");
        }
        public Builder setTask(ChunkStatusTask task) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep.Builder#setTask");
        }
        public ChunkStep build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkStep.Builder#build");
        }
    }
}

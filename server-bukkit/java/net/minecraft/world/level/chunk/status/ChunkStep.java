package net.minecraft.world.level.chunk.status;

import org.basaltmc.bukkit.Unsupported;
public record ChunkStep(ChunkStatus targetStatus, Object directDependencies, Object accumulatedDependencies, int blockStateWriteRadius, Object task) {
    public int getAccumulatedRadiusOf(ChunkStatus status) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.status.ChunkStep.getAccumulatedRadiusOf");
    }
}

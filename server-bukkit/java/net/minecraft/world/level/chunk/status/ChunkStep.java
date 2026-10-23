package net.minecraft.world.level.chunk.status;

public record ChunkStep(ChunkStatus targetStatus, Object directDependencies, Object accumulatedDependencies, int blockStateWriteRadius, Object task) {
    public int getAccumulatedRadiusOf(ChunkStatus status) {
        return 1;
    }
}

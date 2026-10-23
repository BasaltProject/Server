package net.minecraft.world.level.chunk.status;

public record ChunkPyramid(Object steps) {
    public static ChunkPyramid LOADING_PYRAMID = new ChunkPyramid(null);

    public ChunkStep getStepTo(ChunkStatus status) {
        return new ChunkStep(status, null, null, 0, null);
    }
}

package net.minecraft.world.level.chunk.status;

import org.basaltmc.bukkit.Unsupported;
public record ChunkPyramid(Object steps) {
    public static ChunkPyramid LOADING_PYRAMID;
    public ChunkStep getStepTo(ChunkStatus status) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.status.ChunkPyramid.getStepTo");
    }
}

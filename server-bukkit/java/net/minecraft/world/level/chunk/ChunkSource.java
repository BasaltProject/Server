package net.minecraft.world.level.chunk;

import net.minecraft.world.level.lighting.LevelLightEngine;
import org.basaltmc.bukkit.Unsupported;
public abstract class ChunkSource implements LightChunkGetter {
    protected ChunkSource() {
    }
    public LevelLightEngine getLightEngine() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkSource.getLightEngine");
    }
}

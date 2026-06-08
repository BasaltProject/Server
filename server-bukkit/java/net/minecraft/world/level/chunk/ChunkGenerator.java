package net.minecraft.world.level.chunk;

import net.minecraft.world.level.biome.BiomeSource;
import org.basaltmc.bukkit.Unsupported;
public abstract class ChunkGenerator {
    protected ChunkGenerator() {
    }
    public BiomeSource getBiomeSource() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkGenerator.getBiomeSource");
    }
}

package net.minecraft.world.level.levelgen.feature;

import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import org.basaltmc.bukkit.Unsupported;
public interface Feature {
    public default boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.feature.Feature.place");
    }
}

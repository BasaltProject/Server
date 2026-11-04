package org.basaltmc.bukkit.facade;

import net.minecraft.core.BlockPos;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.Feature;

/** A configured feature backed by the world generator's placed feature of the same id. */
public record NativeFeature(Identifier id) implements Feature {
    @Override
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        return NativeFeatures.place(level, id, origin, random.nextLong());
    }
}

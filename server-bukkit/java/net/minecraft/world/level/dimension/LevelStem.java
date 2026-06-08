package net.minecraft.world.level.dimension;

import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record LevelStem(Holder type, ChunkGenerator generator) {
    public static ResourceKey END;
    public static ResourceKey NETHER;
    public static ResourceKey OVERWORLD;
}

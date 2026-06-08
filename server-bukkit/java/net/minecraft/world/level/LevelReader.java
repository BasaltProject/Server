package net.minecraft.world.level;

import net.minecraft.world.level.biome.BiomeResolver;
public interface LevelReader extends BlockAndLightGetter, CollisionGetter, SignalGetter, BiomeResolver {
}

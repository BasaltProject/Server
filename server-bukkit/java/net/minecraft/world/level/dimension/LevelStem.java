// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.dimension;

import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.chunk.ChunkGenerator;
import com.mojang.serialization.Codec;
public record LevelStem(Holder type, ChunkGenerator generator) {
    public static final ResourceKey<LevelStem> OVERWORLD = key("overworld");
    public static final ResourceKey<LevelStem> NETHER = key("the_nether");
    public static final ResourceKey<LevelStem> END = key("the_end");
    private static ResourceKey<LevelStem> key(String name) {
        return ResourceKey.create(net.minecraft.core.registries.Registries.LEVEL_STEM,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }
    public static final Codec<LevelStem> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.LevelStem.class, "CODEC");
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.dimension;

import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.chunk.ChunkGenerator;
import com.mojang.serialization.Codec;
public record LevelStem(Holder type, ChunkGenerator generator) {
    public static ResourceKey END;
    public static ResourceKey NETHER;
    public static ResourceKey OVERWORLD;
    public static final Codec<LevelStem> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.dimension.LevelStem.class, "CODEC");
}

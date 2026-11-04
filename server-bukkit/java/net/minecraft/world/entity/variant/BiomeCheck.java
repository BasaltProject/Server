// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.variant;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.biome.Biome;
public record BiomeCheck(HolderSet<Biome> requiredBiomes) implements SpawnCondition {
    public static final MapCodec<BiomeCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.entity.variant.BiomeCheck.class, "MAP_CODEC");
    public boolean test(SpawnContext context) {
        return requiredBiomes.contains(context.biome());
    }
    public MapCodec<BiomeCheck> codec() {
        return MAP_CODEC;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import net.minecraft.core.Holder;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public class BiomeFilter implements PlacementFilter {
    private static final BiomeFilter INSTANCE = new BiomeFilter();
    public static final MapCodec<BiomeFilter> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.BiomeFilter.class, "CODEC");
    private BiomeFilter() {
        
    }
    public static BiomeFilter biome() {
        return INSTANCE;
    }
    public boolean shouldPlace(PlacementContext context, RandomSource random, BlockPos origin) {
        PlacedFeature top = context.topFeature().orElseThrow(() -> new IllegalStateException("Tried to biome check an unregistered feature, or a feature that should not restrict the biome"));
        Holder<net.minecraft.world.level.biome.Biome> biome = context.getLevel().getBiome(origin);
        return context.generator().getBiomeGenerationSettings(biome).hasFeature(top);
    }
    public MapCodec<BiomeFilter> codec() {
        return CODEC;
    }
}

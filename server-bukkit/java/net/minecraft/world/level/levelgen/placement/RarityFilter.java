// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record RarityFilter(int chance) implements PlacementFilter {
    public static final MapCodec<RarityFilter> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.RarityFilter.class, "CODEC");
    public static RarityFilter onAverageOnceEvery(int chance) {
        return new RarityFilter(chance);
    }
    public boolean shouldPlace(PlacementContext context, RandomSource random, BlockPos origin) {
        return random.nextFloat() < 1.0F / (float) chance;
    }
    public MapCodec<RarityFilter> codec() {
        return CODEC;
    }
}

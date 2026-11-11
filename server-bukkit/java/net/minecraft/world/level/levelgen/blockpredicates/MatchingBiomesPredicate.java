// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.biome.Biome;
public class MatchingBiomesPredicate implements BlockPredicate {
    public static final MapCodec<MatchingBiomesPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.MatchingBiomesPredicate.class, "CODEC");
    private final HolderSet<Biome> biomes;
    public MatchingBiomesPredicate(HolderSet<Biome> biomes) {
        this.biomes = biomes;
    }
    public boolean test(LevelAccessor level, BlockPos origin) {
        return biomes.contains(level.getBiome(origin));
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.MATCHING_BIOMES;
    }
}

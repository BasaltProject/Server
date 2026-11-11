// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record RandomChancePlacement(float chance) implements PlacementFilter {
    public static final MapCodec<RandomChancePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.RandomChancePlacement.class, "CODEC");
    public MapCodec<RandomChancePlacement> codec() {
        return CODEC;
    }
    public boolean shouldPlace(PlacementContext context, RandomSource random, BlockPos origin) {
        return random.nextFloat() < chance;
    }
}

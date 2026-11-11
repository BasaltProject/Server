// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
public record HeightRangePredicate(VerticalAnchor minInclusive, VerticalAnchor maxInclusive) implements BlockPredicate {
    public static final MapCodec<HeightRangePredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.HeightRangePredicate.class, "CODEC");
    public BlockPredicateType<?> type() {
        return BlockPredicateType.HEIGHT_RANGE;
    }
    public boolean test(LevelAccessor level, BlockPos pos) {
        WorldGenerationContext context = WorldGenerationContext.of(level);
        return pos.getY() >= minInclusive.resolveY(context) && pos.getY() <= maxInclusive.resolveY(context);
    }
}

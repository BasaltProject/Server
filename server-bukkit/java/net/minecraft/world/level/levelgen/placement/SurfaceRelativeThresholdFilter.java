// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.Heightmap;
public record SurfaceRelativeThresholdFilter(Heightmap.Types heightmap, int minInclusive, int maxInclusive) implements PlacementFilter {
    public static final MapCodec<SurfaceRelativeThresholdFilter> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.SurfaceRelativeThresholdFilter.class, "CODEC");
    public static SurfaceRelativeThresholdFilter of(Heightmap.Types heightmap, int minInclusive, int maxInclusive) {
        return new SurfaceRelativeThresholdFilter(heightmap, minInclusive, maxInclusive);
    }
    public boolean shouldPlace(PlacementContext context, RandomSource random, BlockPos origin) {
        long surface = context.getHeight(heightmap, origin.getX(), origin.getZ());
        return surface + minInclusive <= origin.getY() && origin.getY() <= surface + maxInclusive;
    }
    public MapCodec<SurfaceRelativeThresholdFilter> codec() {
        return CODEC;
    }
}

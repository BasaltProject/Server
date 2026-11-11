// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record SurfaceWaterDepthFilter(int maxWaterDepth) implements PlacementFilter {
    public static final MapCodec<SurfaceWaterDepthFilter> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.SurfaceWaterDepthFilter.class, "CODEC");
    public static SurfaceWaterDepthFilter forMaxDepth(int maxWaterDepth) {
        return new SurfaceWaterDepthFilter(maxWaterDepth);
    }
    public boolean shouldPlace(PlacementContext context, RandomSource random, BlockPos origin) {
        int floor = context.getHeight(Heightmap.Types.OCEAN_FLOOR, origin.getX(), origin.getZ());
        int surface = context.getHeight(Heightmap.Types.WORLD_SURFACE, origin.getX(), origin.getZ());
        return surface - floor <= maxWaterDepth;


    }
    public MapCodec<SurfaceWaterDepthFilter> codec() {
        return CODEC;
    }
}

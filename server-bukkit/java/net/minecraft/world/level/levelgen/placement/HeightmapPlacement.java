// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.Heightmap;
public record HeightmapPlacement(Heightmap.Types heightmap) implements PlacementModifier {
    public static final MapCodec<HeightmapPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.HeightmapPlacement.class, "CODEC");
    public static HeightmapPlacement onHeightmap(Heightmap.Types heightmap) {
        return new HeightmapPlacement(heightmap);
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        int height = context.getHeight(heightmap, origin.getX(), origin.getZ());
        if (height > context.getMinY()) {
            output.accept(new BlockPos(origin.getX(), height, origin.getZ()));
        }
    }
    public MapCodec<HeightmapPlacement> codec() {
        return CODEC;
    }
}

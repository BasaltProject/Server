// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.levelgen.placement.PlacementContext;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
public record CuboidPlacement(IntProvider xzSize, IntProvider ySize, boolean includeEdges, boolean includeInterior) implements PlacementModifier {
    public static final MapCodec<CuboidPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.CuboidPlacement.class, "CODEC");
    public MapCodec<CuboidPlacement> codec() {
        return CODEC;
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        int sizeY = ySize.sample(random);
        int sizeX = xzSize.sample(random);
        int sizeZ = xzSize.sample(random);
        BlockPos.MutableBlockPos cursor = origin.mutable();
        for (int x = 0; x <= sizeX; x++) {
            for (int y = 0; y <= sizeY; y++) {
                for (int z = 0; z <= sizeZ; z++) {
                    boolean edgeX = x == 0 || x == sizeX;
                    boolean edgeY = y == 0 || y == sizeY;
                    boolean edgeZ = z == 0 || z == sizeZ;
                    int edges = (edgeX ? 1 : 0) + (edgeY ? 1 : 0) + (edgeZ ? 1 : 0);
                    if ((includeEdges || edges < 2) && (includeInterior || edges >= 1)) {
                        cursor.set(origin.getX() + x, origin.getY() + y, origin.getZ() + z);
                        output.accept(cursor.immutable());
                    }
                }
            }
        }


    }
}

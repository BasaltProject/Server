// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
public record CountOnEveryLayerPlacement(IntProvider count) implements PlacementModifier {
    private static boolean isEmpty(BlockState state) {
        return state.isAir() || state.is(Blocks.WATER) || state.is(Blocks.LAVA);
    }
    private static int findOnGround(PlacementContext context, int x, int startY, int z, int wantedLayer) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos(x, startY, z);
        int layer = 0;
        BlockState above = context.getBlockState(cursor);
        for (int y = startY; y >= context.getMinY() + 1; y--) {
            cursor.setY(y - 1);
            BlockState below = context.getBlockState(cursor);
            if (!isEmpty(below) && isEmpty(above) && !below.is(Blocks.BEDROCK)) {
                if (layer == wantedLayer) {
                    return cursor.getY() + 1;
                }
                layer++;
            }
            above = below;
        }
        return Integer.MAX_VALUE;
    }
    public static final MapCodec<CountOnEveryLayerPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.CountOnEveryLayerPlacement.class, "CODEC");
    public static CountOnEveryLayerPlacement of(IntProvider count) {
        return new CountOnEveryLayerPlacement(count);
    }
    public static CountOnEveryLayerPlacement of(int count) {
        return of(net.minecraft.util.valueproviders.ConstantInt.of(count));
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        int layer = 0;
        boolean found;
        do {
            found = false;
            for (int at = 0; at < count.sample(random); at++) {
                int x = random.nextInt(16) + origin.getX();
                int z = random.nextInt(16) + origin.getZ();
                int y = findOnGround(context, x, context.getHeight(Heightmap.Types.MOTION_BLOCKING, x, z), z, layer);
                if (y != Integer.MAX_VALUE) {
                    output.accept(new BlockPos(x, y, z));
                    found = true;
                }
            }
            layer++;
        } while (found);
    }
    public MapCodec<CountOnEveryLayerPlacement> codec() {
        return CODEC;
    }
}

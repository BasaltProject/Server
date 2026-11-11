// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
public class TrunkVineDecorator extends TreeDecorator {
    private static void vineIf(Context context, BlockPos at, net.minecraft.world.level.block.state.properties.BooleanProperty side) {
        if (context.isAir(at)) {
            context.placeVine(at, side);
        }
    }
    public static final MapCodec<TrunkVineDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.TrunkVineDecorator.class, "CODEC");
    public static final TrunkVineDecorator INSTANCE = new TrunkVineDecorator();
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.TRUNK_VINE;
    }
    public void place(TreeDecorator.Context context) {
        RandomSource random = context.random();
        for (BlockPos log : context.logs()) {
            if (random.nextInt(3) > 0) {
                vineIf(context, log.west(), net.minecraft.world.level.block.VineBlock.EAST);
            }
            if (random.nextInt(3) > 0) {
                vineIf(context, log.east(), net.minecraft.world.level.block.VineBlock.WEST);
            }
            if (random.nextInt(3) > 0) {
                vineIf(context, log.north(), net.minecraft.world.level.block.VineBlock.SOUTH);
            }
            if (random.nextInt(3) > 0) {
                vineIf(context, log.south(), net.minecraft.world.level.block.VineBlock.NORTH);
            }
        }
    }
}

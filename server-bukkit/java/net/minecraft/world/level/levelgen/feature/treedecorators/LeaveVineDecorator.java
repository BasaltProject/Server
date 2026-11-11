// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
public class LeaveVineDecorator extends TreeDecorator {
    private final float probability;
    private static void hang(BlockPos start, net.minecraft.world.level.block.state.properties.BooleanProperty side, TreeDecorator.Context context) {
        context.placeVine(start, side);
        BlockPos next = start.below();
        for (int left = 4; context.isAir(next) && left > 0; left--) {
            context.placeVine(next, side);
            next = next.below();
        }
    }
    public static final MapCodec<LeaveVineDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.LeaveVineDecorator.class, "CODEC");
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.LEAVE_VINE;
    }
    public LeaveVineDecorator(float probability) {
        this.probability = probability;
    }
    public void place(TreeDecorator.Context context) {
        RandomSource random = context.random();
        for (BlockPos leaf : context.leaves()) {
            if (random.nextFloat() < probability && context.isAir(leaf.west())) {
                hang(leaf.west(), net.minecraft.world.level.block.VineBlock.EAST, context);
            }
            if (random.nextFloat() < probability && context.isAir(leaf.east())) {
                hang(leaf.east(), net.minecraft.world.level.block.VineBlock.WEST, context);
            }
            if (random.nextFloat() < probability && context.isAir(leaf.north())) {
                hang(leaf.north(), net.minecraft.world.level.block.VineBlock.SOUTH, context);
            }
            if (random.nextFloat() < probability && context.isAir(leaf.south())) {
                hang(leaf.south(), net.minecraft.world.level.block.VineBlock.NORTH, context);
            }
        }
    }
}

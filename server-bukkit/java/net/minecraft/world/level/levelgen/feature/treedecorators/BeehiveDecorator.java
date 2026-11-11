// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import java.util.List;
import com.mojang.serialization.MapCodec;
public class BeehiveDecorator extends TreeDecorator {
    private static final Direction kFacing = Direction.SOUTH;
    private final float probability;
    public static final MapCodec<BeehiveDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.BeehiveDecorator.class, "CODEC");
    public BeehiveDecorator(float probability) {
        this.probability = probability;
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.BEEHIVE;
    }
    public void place(TreeDecorator.Context context) {
        List<BlockPos> leaves = context.leaves();
        List<BlockPos> logs = context.logs();
        if (logs.isEmpty()) {
            return;
        }
        RandomSource random = context.random();
        if (random.nextFloat() >= probability) {
            return;
        }
        int hiveY = !leaves.isEmpty() ? Math.max(leaves.get(0).getY() - 1, logs.get(0).getY() + 1)
            : Math.min(logs.get(0).getY() + 1 + random.nextInt(3), logs.get(logs.size() - 1).getY());
        List<BlockPos> spots = new java.util.ArrayList<>();
        for (BlockPos log : logs) {
            if (log.getY() != hiveY) {
                continue;
            }
            for (Direction side : Direction.Plane.HORIZONTAL) {
                if (side != kFacing.getOpposite()) {
                    spots.add(log.relative(side));
                }
            }
        }
        if (spots.isEmpty()) {
            return;
        }
        net.minecraft.util.Util.shuffle(spots, random);
        for (BlockPos spot : spots) {
            if (context.isAir(spot) && context.isAir(spot.relative(kFacing))) {
                context.setBlock(spot, org.basaltmc.bukkit.facade.StateProps.with(Blocks.BEE_NEST.defaultBlockState(), "facing", kFacing.getSerializedName()));
                int bees = 2 + random.nextInt(2);
                for (int made = 0; made < bees; made++) {
                    random.nextInt(599);
                }
                return;
            }
        }
    }
}

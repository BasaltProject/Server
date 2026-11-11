// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public class AttachedToLeavesDecorator extends TreeDecorator {
    private boolean clearRun(TreeDecorator.Context context, BlockPos leaf, Direction heading) {
        for (int step = 1; step <= requiredEmptyBlocks; step++) {
            if (!context.isAir(leaf.relative(heading, step))) {
                return false;
            }
        }
        return true;
    }
    public static final MapCodec<AttachedToLeavesDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.AttachedToLeavesDecorator.class, "CODEC");
    protected final float probability;
    protected final int exclusionRadiusXZ;
    protected final int exclusionRadiusY;
    protected final Holder<BlockStateProvider> blockProvider;
    protected final int requiredEmptyBlocks;
    protected final List<Direction> directions;
    public AttachedToLeavesDecorator(float probability, int exclusionRadiusXZ, int exclusionRadiusY, Holder<BlockStateProvider> blockProvider, int requiredEmptyBlocks, List<Direction> directions) {
        this.probability = probability;
        this.exclusionRadiusXZ = exclusionRadiusXZ;
        this.exclusionRadiusY = exclusionRadiusY;
        this.blockProvider = blockProvider;
        this.requiredEmptyBlocks = requiredEmptyBlocks;
        this.directions = directions;
    }
    public void place(TreeDecorator.Context context) {
        java.util.Set<BlockPos> taken = new java.util.HashSet<>();
        RandomSource random = context.random();
        for (BlockPos leaf : net.minecraft.util.Util.shuffledCopy(context.leaves(), random)) {
            Direction heading = directions.get(random.nextInt(directions.size()));
            BlockPos target = leaf.relative(heading);
            if (taken.contains(target) || !(random.nextFloat() < probability) || !clearRun(context, leaf, heading)) {
                continue;
            }
            BlockPos low = target.offset(-exclusionRadiusXZ, -exclusionRadiusY, -exclusionRadiusXZ);
            BlockPos high = target.offset(exclusionRadiusXZ, exclusionRadiusY, exclusionRadiusXZ);
            for (BlockPos blocked : BlockPos.betweenClosed(low, high)) {
                taken.add(blocked.immutable());
            }
            context.setBlock(target, blockProvider.value().getState(context.level(), random, target));
        }
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.ATTACHED_TO_LEAVES;
    }
}

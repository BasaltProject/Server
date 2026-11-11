// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public class AttachedToLogsDecorator extends TreeDecorator {
    private final float probability;
    private final Holder<BlockStateProvider> blockProvider;
    private final List<Direction> directions;
    public static final MapCodec<AttachedToLogsDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.AttachedToLogsDecorator.class, "CODEC");
    public AttachedToLogsDecorator(float probability, Holder<BlockStateProvider> blockProvider, List<Direction> directions) {
        this.probability = probability;
        this.blockProvider = blockProvider;
        this.directions = directions;
    }
    public void place(TreeDecorator.Context context) {
        RandomSource random = context.random();
        for (BlockPos log : net.minecraft.util.Util.shuffledCopy(context.logs(), random)) {
            Direction heading = directions.get(random.nextInt(directions.size()));
            BlockPos target = log.relative(heading);
            if (random.nextFloat() <= probability && context.isAir(target)) {
                context.setBlock(target, blockProvider.value().getState(context.level(), random, target));
            }
        }
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.ATTACHED_TO_LOGS;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import java.util.List;
import com.mojang.serialization.MapCodec;
public class CreakingHeartDecorator extends TreeDecorator {
    private final float probability;
    public static final MapCodec<CreakingHeartDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.CreakingHeartDecorator.class, "CODEC");
    public CreakingHeartDecorator(float probability) {
        this.probability = probability;
    }
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.CREAKING_HEART;
    }
    public void place(TreeDecorator.Context context) {
        RandomSource random = context.random();
        List<BlockPos> logs = context.logs();
        if (logs.isEmpty() || random.nextFloat() >= probability) {
            return;
        }
        List<BlockPos> shuffled = new java.util.ArrayList<>(logs);
        net.minecraft.util.Util.shuffle(shuffled, random);
        for (BlockPos candidate : shuffled) {
            boolean surrounded = true;
            for (Direction dir : Direction.values()) {
                if (!context.checkBlock(candidate.relative(dir), state -> state.is(net.minecraft.tags.BlockTags.LOGS))) {
                    surrounded = false;
                    break;
                }
            }
            if (surrounded) {
                BlockState heart = org.basaltmc.bukkit.facade.StateProps.with(Blocks.CREAKING_HEART.defaultBlockState(), "creaking_heart_state", "dormant");
                context.setBlock(candidate, org.basaltmc.bukkit.facade.StateProps.with(heart, "natural", true));
                return;
            }
        }
    }
}

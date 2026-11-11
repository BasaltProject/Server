// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.util.RandomSource;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.Feature;
import java.util.List;
import com.mojang.serialization.MapCodec;
public class PaleMossDecorator extends TreeDecorator {
    private final float leavesProbability;
    private final float trunkProbability;
    private final float groundProbability;
    private static void hangMoss(BlockPos start, TreeDecorator.Context context) {
        BlockPos cursor = start;
        while (context.isAir(cursor.below()) && !((double) context.random().nextFloat() < 0.5)) {
            context.setBlock(cursor, org.basaltmc.bukkit.facade.StateProps.with(Blocks.PALE_HANGING_MOSS.defaultBlockState(), "tip", false));
            cursor = cursor.below();
        }
        context.setBlock(cursor, org.basaltmc.bukkit.facade.StateProps.with(Blocks.PALE_HANGING_MOSS.defaultBlockState(), "tip", true));
    }
    public static final MapCodec<PaleMossDecorator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.treedecorators.PaleMossDecorator.class, "CODEC");
    protected TreeDecoratorType<?> type() {
        return TreeDecoratorType.PALE_MOSS;
    }
    public PaleMossDecorator(float leavesProbability, float trunkProbability, float groundProbability) {
        this.leavesProbability = leavesProbability;
        this.trunkProbability = trunkProbability;
        this.groundProbability = groundProbability;
    }
    public void place(TreeDecorator.Context context) {
        RandomSource random = context.random();
        WorldGenLevel level = context.level();
        List<BlockPos> logs = net.minecraft.util.Util.shuffledCopy(context.logs(), random);
        if (logs.isEmpty()) {
            return;
        }
        BlockPos lowest = java.util.Collections.min(logs, java.util.Comparator.comparingInt(net.minecraft.core.Vec3i::getY));
        if (random.nextFloat() < groundProbability) {
            level.registryAccess().lookup(net.minecraft.core.registries.Registries.FEATURE)
                .flatMap(registry -> registry.get(net.minecraft.data.worldgen.features.VegetationFeatures.PALE_MOSS_PATCH))
                .ifPresent(patch -> ((Feature) patch.value()).place(level, level.getLevel().getChunkSource().getGenerator(), random, lowest.above()));
        }
        for (BlockPos log : context.logs()) {
            if (random.nextFloat() < trunkProbability && context.isAir(log.below())) {
                hangMoss(log.below(), context);
            }
        }
        for (BlockPos leaf : context.leaves()) {
            if (random.nextFloat() < leavesProbability && context.isAir(leaf.below())) {
                hangMoss(leaf.below(), context);
            }
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.core.Direction;
import java.util.function.Function;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class PoplarTrunkPlacer extends TrunkPlacer {
    private final IntProvider trunkHeightAboveBranches;
    private final IntProvider branchAmount;
    private static Function<BlockState, BlockState> along(Direction heading) {
        return state -> state.trySetValue(net.minecraft.world.level.block.RotatedPillarBlock.AXIS, heading.getAxis());
    }
    public static final MapCodec<PoplarTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.PoplarTrunkPlacer.class, "CODEC");
    public PoplarTrunkPlacer(int baseHeight, int heightRandA, int heightRandB, IntProvider trunkHeightAboveBranches, IntProvider branchAmount) {
        super(baseHeight, heightRandA, heightRandB);
        this.trunkHeightAboveBranches = trunkHeightAboveBranches;
        this.branchAmount = branchAmount;
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.POPLAR_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        placeBelowTrunkBlock(level, trunkSetter, random, origin.below(), tree);
        int branchRise = treeHeight - trunkHeightAboveBranches.sample(random);
        for (int rise = 0; rise < treeHeight; rise++) {
            placeLog(level, trunkSetter, random, origin.above(rise), tree);
            List<Direction> sides = new java.util.ArrayList<>();
            for (Direction candidate : Direction.allShuffled(random)) {
                if (!candidate.getAxis().isVertical()) {
                    sides.add(candidate);
                }
            }
            if (branchRise - 1 == rise) {
                int amount = branchAmount.sample(random);
                for (int at = 0; at < amount; at++) {
                    Direction side = sides.get(at);
                    placeLog(level, trunkSetter, random, origin.above(rise).relative(side, 1), tree, along(side));
                }
            }
        }
        return List.of(new FoliagePlacer.FoliageAttachment(origin.above(branchRise), 0, false));
    }
}

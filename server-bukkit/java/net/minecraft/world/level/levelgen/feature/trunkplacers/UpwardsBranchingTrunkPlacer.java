// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class UpwardsBranchingTrunkPlacer extends TrunkPlacer {
    private final IntProvider extraBranchSteps;
    private final float placeBranchPerLogProbability;
    private final IntProvider extraBranchLength;
    private final HolderSet<Block> canGrowThrough;
    private void branchOut(WorldGenLevel level, BiConsumer<BlockPos, BlockState> setter, RandomSource random, int treeHeight, TreeFeature tree,
        List<FoliagePlacer.FoliageAttachment> found, BlockPos.MutableBlockPos cursor, int baseY, Direction heading, int startRise, int stepsLeft) {
        int reachedY = baseY + startRise;
        int curX = cursor.getX();
        int curZ = cursor.getZ();
        int rise = startRise;
        while (rise < treeHeight && stepsLeft > 0) {
            if (rise >= 1) {
                int worldY = baseY + rise;
                curX += heading.getStepX();
                curZ += heading.getStepZ();
                reachedY = worldY;
                if (placeLog(level, setter, random, cursor.set(curX, worldY, curZ), tree)) {
                    reachedY = worldY + 1;
                }
                found.add(new FoliagePlacer.FoliageAttachment(cursor.immutable(), 0, false));
            }
            rise++;
            stepsLeft--;
        }
        if (reachedY - baseY > 1) {
            BlockPos tip = new BlockPos(curX, reachedY, curZ);
            found.add(new FoliagePlacer.FoliageAttachment(tip, 0, false));
            found.add(new FoliagePlacer.FoliageAttachment(tip.below(2), 0, false));
        }
    }
    public static final MapCodec<UpwardsBranchingTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.UpwardsBranchingTrunkPlacer.class, "CODEC");
    public UpwardsBranchingTrunkPlacer(int baseHeight, int heightRandA, int heightRandB, IntProvider extraBranchSteps, float placeBranchPerLogProbability, IntProvider extraBranchLength, HolderSet<Block> canGrowThrough) {
        super(baseHeight, heightRandA, heightRandB);
        this.extraBranchSteps = extraBranchSteps;
        this.placeBranchPerLogProbability = placeBranchPerLogProbability;
        this.extraBranchLength = extraBranchLength;
        this.canGrowThrough = canGrowThrough;
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.UPWARDS_BRANCHING_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>();
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int rise = 0; rise < treeHeight; rise++) {
            int worldY = origin.getY() + rise;
            boolean placed = placeLog(level, trunkSetter, random, cursor.set(origin.getX(), worldY, origin.getZ()), tree);
            if (placed && rise < treeHeight - 1 && random.nextFloat() < placeBranchPerLogProbability) {
                Direction heading = Direction.Plane.HORIZONTAL.getRandomDirection(random);
                int length = extraBranchLength.sample(random);
                int startRise = Math.max(0, length - extraBranchLength.sample(random) - 1);
                int steps = extraBranchSteps.sample(random);
                branchOut(level, trunkSetter, random, treeHeight, tree, found, cursor, worldY, heading, startRise, steps);
            }
            if (rise == treeHeight - 1) {
                found.add(new FoliagePlacer.FoliageAttachment(cursor.set(origin.getX(), worldY + 1, origin.getZ()), 0, false));
            }
        }
        return found;
    }
    protected boolean validTreePos(WorldGenLevel level, BlockPos pos) {
        return super.validTreePos(level, pos) || level.isStateAtPosition(pos, s -> s.is(canGrowThrough));
    }
}

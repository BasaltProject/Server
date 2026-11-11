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
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class CherryTrunkPlacer extends TrunkPlacer {
    private final IntProvider branchCount;
    private final IntProvider branchHorizontalLength;
    private final UniformInt branchStartOffsetFromTop;
    private final UniformInt secondBranchStartOffsetFromTop;
    private final IntProvider branchEndOffsetFromTop;
    private FoliagePlacer.FoliageAttachment growBranch(WorldGenLevel level, BiConsumer<BlockPos, BlockState> setter, RandomSource random, int treeHeight,
        BlockPos origin, TreeFeature tree, Function<BlockState, BlockState> sideways, Direction heading, int startRise, boolean continuesUp, BlockPos.MutableBlockPos cursor) {
        cursor.set(origin).move(Direction.UP, startRise);
        int endRise = treeHeight - 1 + branchEndOffsetFromTop.sample(random);
        boolean pushOut = continuesUp || endRise < startRise;
        int reach = branchHorizontalLength.sample(random) + (pushOut ? 1 : 0);
        BlockPos tip = origin.relative(heading, reach).above(endRise);
        int flat = pushOut ? 2 : 1;
        for (int step = 0; step < flat; step++) {
            placeLog(level, setter, random, cursor.move(heading), tree, sideways);
        }
        Direction vertical = tip.getY() > cursor.getY() ? Direction.UP : Direction.DOWN;
        while (true) {
            int remaining = cursor.distManhattan(tip);
            if (remaining == 0) {
                return new FoliagePlacer.FoliageAttachment(tip.above(), 0, false);
            }
            float verticalShare = (float) Math.abs(tip.getY() - cursor.getY()) / (float) remaining;
            boolean goUp = random.nextFloat() < verticalShare;
            cursor.move(goUp ? vertical : heading);
            placeLog(level, setter, random, cursor, tree, goUp ? Function.identity() : sideways);
        }
    }
    public static final MapCodec<CherryTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.CherryTrunkPlacer.class, "CODEC");
    public CherryTrunkPlacer(int baseHeight, int heightRandA, int heightRandB, IntProvider branchCount, IntProvider branchHorizontalLength, UniformInt branchStartOffsetFromTop, IntProvider branchEndOffsetFromTop) {
        super(baseHeight, heightRandA, heightRandB);
        this.branchCount = branchCount;
        this.branchHorizontalLength = branchHorizontalLength;
        this.branchStartOffsetFromTop = branchStartOffsetFromTop;
        this.secondBranchStartOffsetFromTop = UniformInt.of(branchStartOffsetFromTop.minInclusive(), branchStartOffsetFromTop.maxInclusive() - 1);
        this.branchEndOffsetFromTop = branchEndOffsetFromTop;
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.CHERRY_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        placeBelowTrunkBlock(level, trunkSetter, random, origin.below(), tree);
        int firstStart = Math.max(0, treeHeight - 1 + branchStartOffsetFromTop.sample(random));
        int secondStart = Math.max(0, treeHeight - 1 + secondBranchStartOffsetFromTop.sample(random));
        if (secondStart >= firstStart) {
            secondStart++;
        }
        int branches = branchCount.sample(random);
        boolean middle = branches == 3;
        boolean both = branches >= 2;
        int trunkTop = middle ? treeHeight : both ? Math.max(firstStart, secondStart) + 1 : firstStart + 1;
        for (int rise = 0; rise < trunkTop; rise++) {
            placeLog(level, trunkSetter, random, origin.above(rise), tree);
        }
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>();
        if (middle) {
            found.add(new FoliagePlacer.FoliageAttachment(origin.above(trunkTop), 0, false));
        }
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        Direction heading = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        Function<BlockState, BlockState> sideways = state -> state.trySetValue(net.minecraft.world.level.block.RotatedPillarBlock.AXIS, heading.getAxis());
        found.add(growBranch(level, trunkSetter, random, treeHeight, origin, tree, sideways, heading, firstStart, firstStart < trunkTop - 1, cursor));
        if (both) {
            found.add(growBranch(level, trunkSetter, random, treeHeight, origin, tree, sideways, heading.getOpposite(), secondStart, secondStart < trunkTop - 1, cursor));
        }
        return found;
    }
}

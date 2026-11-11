// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.core.Direction;
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
public class BendingTrunkPlacer extends TrunkPlacer {
    private final int minHeightForLeaves;
    private final IntProvider bendLength;
    public static final MapCodec<BendingTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.BendingTrunkPlacer.class, "CODEC");
    public BendingTrunkPlacer(int baseHeight, int heightRandA, int heightRandB, int minHeightForLeaves, IntProvider bendLength) {
        super(baseHeight, heightRandA, heightRandB);
        this.minHeightForLeaves = minHeightForLeaves;
        this.bendLength = bendLength;
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.BENDING_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        Direction bend = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        int lastLog = treeHeight - 1;
        BlockPos.MutableBlockPos cursor = origin.mutable();
        placeBelowTrunkBlock(level, trunkSetter, random, cursor.below(), tree);
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>();
        for (int rise = 0; rise <= lastLog; rise++) {
            if (rise + 1 >= lastLog + random.nextInt(2)) {
                cursor.move(bend);
            }
            if (TreeFeature.validTreePos(level, cursor)) {
                placeLog(level, trunkSetter, random, cursor, tree);
            }
            if (rise >= minHeightForLeaves) {
                found.add(new FoliagePlacer.FoliageAttachment(cursor.immutable(), 0, false));
            }
            cursor.move(Direction.UP);
        }
        int sideways = bendLength.sample(random);
        for (int step = 0; step <= sideways; step++) {
            if (TreeFeature.validTreePos(level, cursor)) {
                placeLog(level, trunkSetter, random, cursor, tree);
            }
            found.add(new FoliagePlacer.FoliageAttachment(cursor.immutable(), 0, false));
            cursor.move(bend);
        }
        return found;
    }
}

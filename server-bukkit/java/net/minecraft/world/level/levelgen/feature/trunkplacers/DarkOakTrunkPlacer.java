// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class DarkOakTrunkPlacer extends TrunkPlacer {
    public static final MapCodec<DarkOakTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.DarkOakTrunkPlacer.class, "CODEC");
    public DarkOakTrunkPlacer(int baseHeight, int heightRandA, int heightRandB) {
        super(baseHeight, heightRandA, heightRandB);
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.DARK_OAK_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>();
        BlockPos ground = origin.below();
        placeBelowTrunkBlock(level, trunkSetter, random, ground, tree);
        placeBelowTrunkBlock(level, trunkSetter, random, ground.east(), tree);
        placeBelowTrunkBlock(level, trunkSetter, random, ground.south(), tree);
        placeBelowTrunkBlock(level, trunkSetter, random, ground.south().east(), tree);
        Direction lean = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        int leanStart = treeHeight - random.nextInt(4);
        int leanLeft = 2 - random.nextInt(3);
        int baseX = origin.getX();
        int baseY = origin.getY();
        int baseZ = origin.getZ();
        int curX = baseX;
        int curZ = baseZ;
        int topY = baseY + treeHeight - 1;
        for (int rise = 0; rise < treeHeight; rise++) {
            if (rise >= leanStart && leanLeft > 0) {
                curX += lean.getStepX();
                curZ += lean.getStepZ();
                leanLeft--;
            }
            BlockPos column = new BlockPos(curX, baseY + rise, curZ);
            if (TreeFeature.isAirOrLeaves(level, column)) {
                placeLog(level, trunkSetter, random, column, tree);
                placeLog(level, trunkSetter, random, column.east(), tree);
                placeLog(level, trunkSetter, random, column.south(), tree);
                placeLog(level, trunkSetter, random, column.east().south(), tree);
            }
        }
        found.add(new FoliagePlacer.FoliageAttachment(new BlockPos(curX, topY, curZ), 0, true));
        for (int offX = -1; offX <= 2; offX++) {
            for (int offZ = -1; offZ <= 2; offZ++) {
                boolean insideCore = offX >= 0 && offX <= 1 && offZ >= 0 && offZ <= 1;
                if (!insideCore && random.nextInt(3) <= 0) {
                    int drop = random.nextInt(3) + 2;
                    for (int below = 0; below < drop; below++) {
                        placeLog(level, trunkSetter, random, new BlockPos(baseX + offX, topY - below - 1, baseZ + offZ), tree);
                    }
                    found.add(new FoliagePlacer.FoliageAttachment(new BlockPos(baseX + offX, topY, baseZ + offZ), 0, false));
                }
            }
        }
        return found;
    }
}

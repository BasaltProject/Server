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
public class ForkingTrunkPlacer extends TrunkPlacer {
    private static int stepAlong(Direction heading, int start, boolean xAxis) {
        return start + (xAxis ? heading.getStepX() : heading.getStepZ());
    }
    public static final MapCodec<ForkingTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.ForkingTrunkPlacer.class, "CODEC");
    public ForkingTrunkPlacer(int baseHeight, int heightRandA, int heightRandB) {
        super(baseHeight, heightRandA, heightRandB);
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.FORKING_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        placeBelowTrunkBlock(level, trunkSetter, random, origin.below(), tree);
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>();
        Direction lean = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        int leanStart = treeHeight - random.nextInt(4) - 1;
        int leanLeft = 3 - random.nextInt(3);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        int curX = origin.getX();
        int curZ = origin.getZ();
        java.util.OptionalInt top = java.util.OptionalInt.empty();
        for (int rise = 0; rise < treeHeight; rise++) {
            int worldY = origin.getY() + rise;
            if (rise >= leanStart && leanLeft > 0) {
                curX = stepAlong(lean, curX, true);
                curZ = stepAlong(lean, curZ, false);
                leanLeft--;
            }
            if (placeLog(level, trunkSetter, random, cursor.set(curX, worldY, curZ), tree)) {
                top = java.util.OptionalInt.of(worldY + 1);
            }
        }
        if (top.isPresent()) {
            found.add(new FoliagePlacer.FoliageAttachment(new BlockPos(curX, top.getAsInt(), curZ), 1, false));
        }
        curX = origin.getX();
        curZ = origin.getZ();
        Direction side = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        if (side != lean) {
            int rise = leanStart - random.nextInt(2) - 1;
            int sideLeft = 1 + random.nextInt(3);
            top = java.util.OptionalInt.empty();
            while (rise < treeHeight && sideLeft > 0) {
                if (rise >= 1) {
                    int worldY = origin.getY() + rise;
                    curX = stepAlong(side, curX, true);
                    curZ = stepAlong(side, curZ, false);
                    if (placeLog(level, trunkSetter, random, cursor.set(curX, worldY, curZ), tree)) {
                        top = java.util.OptionalInt.of(worldY + 1);
                    }
                }
                rise++;
                sideLeft--;
            }
            if (top.isPresent()) {
                found.add(new FoliagePlacer.FoliageAttachment(new BlockPos(curX, top.getAsInt(), curZ), 0, false));
            }
        }
        return found;
    }
}

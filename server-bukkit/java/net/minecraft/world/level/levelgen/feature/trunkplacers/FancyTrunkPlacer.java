// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class FancyTrunkPlacer extends TrunkPlacer {
    private static float crownRadius(int height, int y) {
        if ((float) y < (float) height * 0.3F) {
            return -1.0F;
        }
        float half = (float) height / 2.0F;
        float gap = half - (float) y;
        if (Math.abs(gap) >= half && gap != 0.0F) {
            return 0.0F;
        }
        float chord = gap == 0.0F ? half : Mth.sqrt(half * half - gap * gap);
        return chord * 0.5F;
    }
    private boolean keepsBranch(int height, int localY) {
        return (double) localY >= (double) height * 0.2;
    }
    private static int longestAxis(BlockPos delta) {
        return Math.max(Mth.abs(delta.getX()), Math.max(Mth.abs(delta.getY()), Mth.abs(delta.getZ())));
    }
    private static Direction.Axis dominantAxis(BlockPos from, BlockPos to) {
        int along = Math.abs(to.getX() - from.getX());
        int across = Math.abs(to.getZ() - from.getZ());
        int widest = Math.max(along, across);
        if (widest <= 0) {
            return Direction.Axis.Y;
        }
        return along == widest ? Direction.Axis.X : Direction.Axis.Z;
    }
    private boolean limb(WorldGenLevel level, BiConsumer<BlockPos, BlockState> setter, RandomSource random, BlockPos from, BlockPos to, boolean place, TreeFeature tree) {
        if (!place && java.util.Objects.equals(from, to)) {
            return true;
        }
        BlockPos delta = to.offset(-from.getX(), -from.getY(), -from.getZ());
        int steps = longestAxis(delta);
        float stepX = (float) delta.getX() / (float) steps;
        float stepY = (float) delta.getY() / (float) steps;
        float stepZ = (float) delta.getZ() / (float) steps;
        for (int at = 0; at <= steps; at++) {
            BlockPos here = from.offset(Mth.floor(0.5F + (float) at * stepX), Mth.floor(0.5F + (float) at * stepY), Mth.floor(0.5F + (float) at * stepZ));
            if (place) {
                placeLog(level, setter, random, here, tree, state -> state.trySetValue(net.minecraft.world.level.block.RotatedPillarBlock.AXIS, dominantAxis(from, here)));
            } else if (!isFree(level, here)) {
                return false;
            }
        }
        return true;
    }
    private void branches(WorldGenLevel level, BiConsumer<BlockPos, BlockState> setter, RandomSource random, int height, BlockPos origin, List<FoliageCoords> coords, TreeFeature tree) {
        for (FoliageCoords coord : coords) {
            BlockPos base = new BlockPos(origin.getX(), coord.branchBase, origin.getZ());
            if (!base.equals(coord.attachment.pos()) && keepsBranch(height, coord.branchBase - origin.getY())) {
                limb(level, setter, random, base, coord.attachment.pos(), true, tree);
            }
        }
    }
    public static final MapCodec<FancyTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.FancyTrunkPlacer.class, "CODEC");
    public FancyTrunkPlacer(int baseHeight, int heightRandA, int heightRandB) {
        super(baseHeight, heightRandA, heightRandB);
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.FANCY_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        int height = treeHeight + 2;
        int trunkHeight = Mth.floor((double) height * 0.618);
        placeBelowTrunkBlock(level, trunkSetter, random, origin.below(), tree);
        int perLayer = Math.min(1, Mth.floor(1.382 + Math.pow(1.0 * (double) height / 13.0, 2.0)));
        int trunkTop = origin.getY() + trunkHeight;
        int layer = height - 5;
        List<FoliageCoords> coords = new java.util.ArrayList<>();
        coords.add(new FoliageCoords(origin.above(layer), trunkTop));
        for (; layer >= 0; layer--) {
            float shape = crownRadius(height, layer);
            if (shape < 0.0F) {
                continue;
            }
            for (int each = 0; each < perLayer; each++) {
                double radius = 1.0 * (double) shape * ((double) random.nextFloat() + 0.328);
                double angle = (double) (random.nextFloat() * 2.0F) * Math.PI;
                double offX = radius * Math.sin(angle) + 0.5;
                double offZ = radius * Math.cos(angle) + 0.5;
                BlockPos start = origin.offset(Mth.floor(offX), layer - 1, Mth.floor(offZ));
                BlockPos end = start.above(5);
                if (!limb(level, trunkSetter, random, start, end, false, tree)) {
                    continue;
                }
                int dx = origin.getX() - start.getX();
                int dz = origin.getZ() - start.getZ();
                double slopeY = (double) start.getY() - Math.sqrt((double) (dx * dx + dz * dz)) * 0.381;
                int baseY = slopeY > (double) trunkTop ? trunkTop : (int) slopeY;
                BlockPos base = new BlockPos(origin.getX(), baseY, origin.getZ());
                if (limb(level, trunkSetter, random, base, start, false, tree)) {
                    coords.add(new FoliageCoords(start, base.getY()));
                }
            }
        }
        limb(level, trunkSetter, random, origin, origin.above(trunkHeight), true, tree);
        branches(level, trunkSetter, random, height, origin, coords, tree);
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>();
        for (FoliageCoords coord : coords) {
            if (keepsBranch(height, coord.branchBase - origin.getY())) {
                found.add(coord.attachment);
            }
        }
        return found;


    }
    static class FoliageCoords {
        private final FoliagePlacer.FoliageAttachment attachment;
        private final int branchBase;
        public FoliageCoords(BlockPos pos, int branchBase) {
            this.attachment = new FoliagePlacer.FoliageAttachment(pos, 0, false);
            this.branchBase = branchBase;
            
        }
        public int getBranchBase() {
            return branchBase;
        }
    }
}

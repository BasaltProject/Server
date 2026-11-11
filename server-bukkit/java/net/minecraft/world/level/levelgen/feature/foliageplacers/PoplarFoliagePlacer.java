// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import java.util.function.Function;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class PoplarFoliagePlacer extends FoliagePlacer {
    private final IntProvider height;
    private final float sideHoleChance;
    private static Function<BlockState, BlockState> along(Direction heading) {
        return state -> state.trySetValue(net.minecraft.world.level.block.RotatedPillarBlock.AXIS, heading.getAxis());
    }
    private static boolean insideRhombus(int ring, int absX, int absZ, int cut, int extraCut) {
        return absX + absZ <= ring * 2 - (cut + extraCut);
    }
    private static boolean lowerLeftOrUpperRight(int dx, int dz) {
        return dx > 0 && dz < 0 || dz > 0 && dx < 0;
    }
    private static boolean upperLeftOrLowerRight(int dx, int dz) {
        return dx > 0 && dz > 0 || dz < 0 && dx < 0;
    }
    private static boolean partialRow(int foliageHeight, int y) {
        return foliageHeight - 1 == y || foliageHeight - 2 == y;
    }
    private int cornerCut(int dx, int dz, int ring, boolean partial, boolean flip) {
        boolean smallCorner = flip ? upperLeftOrLowerRight(dx, dz) : lowerLeftOrUpperRight(dx, dz);
        if (smallCorner) {
            return ring - 1;
        }
        return partial ? ring + 1 : ring;
    }
    private static void swapForLog(WorldGenLevel level, FoliagePlacer.FoliageSetter setter, RandomSource random, TreeFeature tree, BlockPos pos, Function<BlockState, BlockState> modifier) {
        BlockState leaf = ((BlockStateProvider) tree.foliageProvider().value()).getState(level, random, pos);
        if (level.isStateAtPosition(pos, state -> state.equals(leaf))) {
            setter.set(pos, modifier.apply(((BlockStateProvider) tree.trunkProvider().value()).getState(level, random, pos)));
        }
    }
    private void logsInRow(WorldGenLevel level, FoliagePlacer.FoliageSetter setter, TreeFeature tree, RandomSource random, BlockPos origin, int ring, int y, boolean wide, int foliageHeight, boolean flip) {
        int extra = wide ? 1 : 0;
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int dx = -ring; dx <= ring + extra; dx++) {
            for (int dz = -ring; dz <= ring + extra; dz++) {
                int absZ = Mth.abs(dz);
                int absX = Mth.abs(dx);
                int cut = cornerCut(dx, dz, ring, partialRow(foliageHeight, y), flip);
                boolean spine = absZ == 0 && ring - absX >= 4 || absX == 0 && ring - absZ >= 4;
                if (insideRhombus(ring, absX, absZ, cut, 2) && spine) {
                    cursor.setWithOffset(origin, dx, y, dz);
                    swapForLog(level, setter, random, tree, cursor,
                        along(Direction.fromAxisAndDirection(absZ == 0 ? Direction.Axis.X : Direction.Axis.Z, Direction.AxisDirection.POSITIVE)));
                }
            }
        }
    }
    private void leafRow(WorldGenLevel level, FoliagePlacer.FoliageSetter setter, RandomSource random, TreeFeature tree, BlockPos origin, int ring, int y, boolean wide, int foliageHeight, boolean flip) {
        int extra = wide ? 1 : 0;
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int dx = -ring; dx <= ring + extra; dx++) {
            for (int dz = -ring; dz <= ring + extra; dz++) {
                boolean partial = partialRow(foliageHeight, y);
                int cut = cornerCut(dx, dz, ring, partial, flip);
                int absX = Mth.abs(dx);
                int absZ = Mth.abs(dz);
                boolean skip;
                if (partial && (absX == ring || absZ == ring)) {
                    skip = true;
                } else {
                    int extraCut = random.nextFloat() <= sideHoleChance ? 1 : 0;
                    skip = !insideRhombus(ring, absX, absZ, cut, extraCut);
                }
                if (!skip) {
                    cursor.setWithOffset(origin, dx, y, dz);
                    tryPlaceLeaf(level, setter, random, tree, cursor);
                }
            }
        }
    }
    public static final MapCodec<PoplarFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.PoplarFoliagePlacer.class, "CODEC");
    public PoplarFoliagePlacer(IntProvider radius, IntProvider offset, IntProvider height, float sideHoleChance) {
        super(radius, offset);
        this.height = height;
        this.sideHoleChance = sideHoleChance;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.POPLAR_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        boolean wide = foliageAttachment.doubleTrunk();
        BlockPos center = foliageAttachment.pos().above(offset);
        int ring = leafRadius + foliageAttachment.radiusOffsetXZ() - 1;
        boolean flip = random.nextBoolean();
        int span = foliageHeight + foliageAttachment.foliageHeightOffset();
        leafRow(level, foliageSetter, random, tree, center, ring - 2, span - 1, wide, span, flip);
        leafRow(level, foliageSetter, random, tree, center, ring - 1, span - 2, wide, span, flip);
        leafRow(level, foliageSetter, random, tree, center, ring - 1, span - 3, wide, span, flip);
        for (int row = span - 4; row >= 1; row--) {
            leafRow(level, foliageSetter, random, tree, center, ring, row, wide, span, flip);
        }
        logsInRow(level, foliageSetter, tree, random, center, ring, span - 4, wide, span, flip);
        leafRow(level, foliageSetter, random, tree, center, ring - 1, 0, wide, span, flip);
        leafRow(level, foliageSetter, random, tree, center, Mth.clamp(ring - 2, 1, 2), -1, wide, span, flip);
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return height.sample(random);
    }
    protected boolean shouldSkipLocationSigned(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        throw new IllegalStateException("Overridden method needs more context");
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        throw new IllegalStateException("Overridden method needs more context");


    }
}

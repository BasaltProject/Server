// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class CherryFoliagePlacer extends FoliagePlacer {
    private final IntProvider height;
    private final float wideBottomLayerHoleChance;
    private final float cornerHoleChance;
    private final float hangingLeavesChance;
    private final float hangingLeavesExtensionChance;
    public static final MapCodec<CherryFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.CherryFoliagePlacer.class, "CODEC");
    public CherryFoliagePlacer(IntProvider radius, IntProvider offset, IntProvider height, float wideBottomLayerHoleChance, float cornerHoleChance, float hangingLeavesChance, float hangingLeavesExtensionChance) {
        super(radius, offset);
        this.height = height;
        this.wideBottomLayerHoleChance = wideBottomLayerHoleChance;
        this.cornerHoleChance = cornerHoleChance;
        this.hangingLeavesChance = hangingLeavesChance;
        this.hangingLeavesExtensionChance = hangingLeavesExtensionChance;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.CHERRY_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        boolean wide = foliageAttachment.doubleTrunk();
        BlockPos center = foliageAttachment.pos().above(offset);
        int ring = leafRadius + foliageAttachment.radiusOffsetXZ() - 1;
        int span = foliageHeight + foliageAttachment.foliageHeightOffset();
        placeLeavesRow(level, foliageSetter, random, tree, center, ring - 2, span - 3, wide);
        placeLeavesRow(level, foliageSetter, random, tree, center, ring - 1, span - 4, wide);
        for (int row = span - 5; row >= 0; row--) {
            placeLeavesRow(level, foliageSetter, random, tree, center, ring, row, wide);
        }
        placeLeavesRowWithHangingLeavesBelow(level, foliageSetter, random, tree, center, ring, -1, wide, hangingLeavesChance, hangingLeavesExtensionChance);
        placeLeavesRowWithHangingLeavesBelow(level, foliageSetter, random, tree, center, ring - 1, -2, wide, hangingLeavesChance, hangingLeavesExtensionChance);
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return height.sample(random);
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        if (y == -1 && (dx == currentRadius || dz == currentRadius) && random.nextFloat() < wideBottomLayerHoleChance) {
            return true;
        }
        boolean corner = dx == currentRadius && dz == currentRadius;
        if (currentRadius > 2) {
            return corner || dx + dz > currentRadius * 2 - 2 && random.nextFloat() < cornerHoleChance;
        }
        return corner && random.nextFloat() < cornerHoleChance;
    }
}

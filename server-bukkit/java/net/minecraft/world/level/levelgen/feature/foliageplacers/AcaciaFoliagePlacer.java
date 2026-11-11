// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class AcaciaFoliagePlacer extends FoliagePlacer {
    public static final MapCodec<AcaciaFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.AcaciaFoliagePlacer.class, "CODEC");
    public AcaciaFoliagePlacer(IntProvider radius, IntProvider offset) {
        super(radius, offset);
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.ACACIA_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        boolean wide = foliageAttachment.doubleTrunk();
        BlockPos center = foliageAttachment.pos().above(offset);
        int span = foliageHeight + foliageAttachment.foliageHeightOffset();
        placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + foliageAttachment.radiusOffsetXZ(), -1 - span, wide);
        placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius - 1, -span, wide);
        placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + foliageAttachment.radiusOffsetXZ() - 1, 0, wide);
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return 0;
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        if (y == 0) {
            return (dx > 1 || dz > 1) && dx != 0 && dz != 0;
        }
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    }
}

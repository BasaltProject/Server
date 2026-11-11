// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class DarkOakFoliagePlacer extends FoliagePlacer {
    public static final MapCodec<DarkOakFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.DarkOakFoliagePlacer.class, "CODEC");
    public DarkOakFoliagePlacer(IntProvider radius, IntProvider offset) {
        super(radius, offset);
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.DARK_OAK_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        BlockPos center = foliageAttachment.pos().above(offset);
        boolean wide = foliageAttachment.doubleTrunk();
        if (!wide) {
            placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + 2, -1, wide);
            placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + 1, 0, wide);
            return;
        }
        placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + 2, -1, wide);
        placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + 3, 0, wide);
        placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius + 2, 1, wide);
        if (random.nextBoolean()) {
            placeLeavesRow(level, foliageSetter, random, tree, center, leafRadius, 2, wide);
        }
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return 4;
    }
    protected boolean shouldSkipLocationSigned(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        boolean cornerCut = y == 0 && doubleTrunk && (dx == -currentRadius || dx >= currentRadius) && (dz == -currentRadius || dz >= currentRadius);
        return cornerCut || super.shouldSkipLocationSigned(random, dx, y, dz, currentRadius, doubleTrunk);
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        if (y == -1 && !doubleTrunk) {
            return dx == currentRadius && dz == currentRadius;
        }
        return y == 1 && dx + dz > currentRadius * 2 - 2;
    }
}

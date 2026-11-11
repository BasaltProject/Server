// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class RandomSpreadFoliagePlacer extends FoliagePlacer {
    private final IntProvider foliageHeight;
    private final int leafPlacementAttempts;
    public static final MapCodec<RandomSpreadFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.RandomSpreadFoliagePlacer.class, "CODEC");
    public RandomSpreadFoliagePlacer(IntProvider radius, IntProvider offset, IntProvider foliageHeight, int leafPlacementAttempts) {
        super(radius, offset);
        this.foliageHeight = foliageHeight;
        this.leafPlacementAttempts = leafPlacementAttempts;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.RANDOM_SPREAD_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        BlockPos center = foliageAttachment.pos();
        BlockPos.MutableBlockPos cursor = center.mutable();
        for (int attempt = 0; attempt < leafPlacementAttempts; attempt++) {
            int shiftX = random.nextInt(leafRadius) - random.nextInt(leafRadius);
            int shiftY = random.nextInt(foliageHeight) - random.nextInt(foliageHeight);
            int shiftZ = random.nextInt(leafRadius) - random.nextInt(leafRadius);
            cursor.setWithOffset(center, shiftX, shiftY, shiftZ);
            tryPlaceLeaf(level, foliageSetter, random, tree, cursor);
        }
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return this.foliageHeight.sample(random);
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        return false;
    }
}

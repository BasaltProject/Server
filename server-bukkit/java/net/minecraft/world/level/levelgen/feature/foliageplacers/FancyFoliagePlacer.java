// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class FancyFoliagePlacer extends BlobFoliagePlacer {
    public static final MapCodec<FancyFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.FancyFoliagePlacer.class, "CODEC");
    public FancyFoliagePlacer(IntProvider radius, IntProvider offset, int height) {
        super(radius, offset, height);
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.FANCY_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        for (int row = offset; row >= offset - foliageHeight; row--) {
            boolean inner = row != offset && row != offset - foliageHeight;
            placeLeavesRow(level, foliageSetter, random, tree, foliageAttachment.pos(), leafRadius + (inner ? 1 : 0), row, foliageAttachment.doubleTrunk());
        }
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        float cx = (float) dx + 0.5F;
        float cz = (float) dz + 0.5F;
        return cx * cx + cz * cz > (float) (currentRadius * currentRadius);
    }
}

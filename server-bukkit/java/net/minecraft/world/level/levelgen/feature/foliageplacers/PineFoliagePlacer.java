// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class PineFoliagePlacer extends FoliagePlacer {
    private final IntProvider height;
    public static final MapCodec<PineFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.PineFoliagePlacer.class, "CODEC");
    public PineFoliagePlacer(IntProvider radius, IntProvider offset, IntProvider height) {
        super(radius, offset);
        this.height = height;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.PINE_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        int ring = 0;
        int span = foliageHeight + foliageAttachment.foliageHeightOffset();
        for (int row = offset; row >= offset - span; row--) {
            placeLeavesRow(level, foliageSetter, random, tree, foliageAttachment.pos(), ring, row, foliageAttachment.doubleTrunk());
            if (ring >= 1 && row == offset - span + 1) {
                ring--;
            } else if (ring < leafRadius + foliageAttachment.radiusOffsetXZ()) {
                ring++;
            }
        }
    }
    public int foliageRadius(RandomSource random, int trunkHeight) {
        return super.foliageRadius(random, trunkHeight) + random.nextInt(Math.max(trunkHeight + 1, 1));
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return height.sample(random);
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    }
}

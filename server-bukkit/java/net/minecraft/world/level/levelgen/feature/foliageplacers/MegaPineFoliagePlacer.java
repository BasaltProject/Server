// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class MegaPineFoliagePlacer extends FoliagePlacer {
    private final IntProvider crownHeight;
    public static final MapCodec<MegaPineFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.MegaPineFoliagePlacer.class, "CODEC");
    public MegaPineFoliagePlacer(IntProvider radius, IntProvider offset, IntProvider crownHeight) {
        super(radius, offset);
        this.crownHeight = crownHeight;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.MEGA_PINE_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        BlockPos center = foliageAttachment.pos();
        int lastSmooth = 0;
        int span = foliageHeight + foliageAttachment.foliageHeightOffset();
        for (int worldY = center.getY() - span + offset; worldY <= center.getY() + offset; worldY++) {
            int depth = center.getY() - worldY;
            int smooth = leafRadius + foliageAttachment.radiusOffsetXZ() + Mth.floor((float) depth / (float) span * 3.5F);
            int ring = depth > 0 && smooth == lastSmooth && (worldY & 1) == 0 ? smooth + 1 : smooth;
            placeLeavesRow(level, foliageSetter, random, tree, new BlockPos(center.getX(), worldY, center.getZ()), ring, 0, foliageAttachment.doubleTrunk());
            lastSmooth = smooth;
        }
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return crownHeight.sample(random);
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        return dx + dz >= 7 || dx * dx + dz * dz > currentRadius * currentRadius;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class MegaJungleFoliagePlacer extends FoliagePlacer {
    public static final MapCodec<MegaJungleFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.MegaJungleFoliagePlacer.class, "CODEC");
    protected final int height;
    public MegaJungleFoliagePlacer(IntProvider radius, IntProvider offset, int height) {
        super(radius, offset);
        this.height = height;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.MEGA_JUNGLE_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        int span = (foliageAttachment.doubleTrunk() ? foliageHeight : 1 + random.nextInt(2)) + foliageAttachment.foliageHeightOffset();
        for (int row = offset; row >= offset - span; row--) {
            int ring = leafRadius + foliageAttachment.radiusOffsetXZ() + 1 - row;
            placeLeavesRow(level, foliageSetter, random, tree, foliageAttachment.pos(), ring, row, foliageAttachment.doubleTrunk());
        }
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return height;
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        return dx + dz >= 7 || dx * dx + dz * dz > currentRadius * currentRadius;
    }
}

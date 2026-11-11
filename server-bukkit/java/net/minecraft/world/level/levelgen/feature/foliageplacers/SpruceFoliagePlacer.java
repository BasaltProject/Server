// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public class SpruceFoliagePlacer extends FoliagePlacer {
    private final IntProvider trunkHeight;
    public static final MapCodec<SpruceFoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.foliageplacers.SpruceFoliagePlacer.class, "CODEC");
    public SpruceFoliagePlacer(IntProvider radius, IntProvider offset, IntProvider trunkHeight) {
        super(radius, offset);
        this.trunkHeight = trunkHeight;
    }
    protected FoliagePlacerType<?> type() {
        return FoliagePlacerType.SPRUCE_FOLIAGE_PLACER;
    }
    protected void createFoliage(WorldGenLevel level, FoliagePlacer.FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliagePlacer.FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset) {
        BlockPos center = foliageAttachment.pos();
        int ring = random.nextInt(2);
        int ceiling = 1;
        int floor = 0;
        int span = foliageHeight + foliageAttachment.foliageHeightOffset();
        for (int row = offset; row >= -span; row--) {
            placeLeavesRow(level, foliageSetter, random, tree, center, ring, row, foliageAttachment.doubleTrunk());
            if (ring >= ceiling) {
                ring = floor;
                floor = 1;
                ceiling = Math.min(ceiling + 1, leafRadius + foliageAttachment.radiusOffsetXZ());
            } else {
                ring++;
            }
        }
    }
    public int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree) {
        return Math.max(4, treeHeight - trunkHeight.sample(random));
    }
    protected boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        return dx == currentRadius && dz == currentRadius && currentRadius > 0;
    }
}

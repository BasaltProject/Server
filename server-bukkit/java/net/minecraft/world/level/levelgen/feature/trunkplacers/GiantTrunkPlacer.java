// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class GiantTrunkPlacer extends TrunkPlacer {
    private void logAt(WorldGenLevel level, BiConsumer<BlockPos, BlockState> setter, RandomSource random, BlockPos.MutableBlockPos cursor, TreeFeature tree, BlockPos base, int dx, int dy, int dz) {
        cursor.setWithOffset(base, dx, dy, dz);
        placeLogIfFree(level, setter, random, cursor, tree);
    }
    public static final MapCodec<GiantTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.GiantTrunkPlacer.class, "CODEC");
    public GiantTrunkPlacer(int baseHeight, int heightRandA, int heightRandB) {
        super(baseHeight, heightRandA, heightRandB);
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.GIANT_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        BlockPos ground = origin.below();
        placeBelowTrunkBlock(level, trunkSetter, random, ground, tree);
        placeBelowTrunkBlock(level, trunkSetter, random, ground.east(), tree);
        placeBelowTrunkBlock(level, trunkSetter, random, ground.south(), tree);
        placeBelowTrunkBlock(level, trunkSetter, random, ground.south().east(), tree);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int rise = 0; rise < treeHeight; rise++) {
            logAt(level, trunkSetter, random, cursor, tree, origin, 0, rise, 0);
            if (rise < treeHeight - 1) {
                logAt(level, trunkSetter, random, cursor, tree, origin, 1, rise, 0);
                logAt(level, trunkSetter, random, cursor, tree, origin, 1, rise, 1);
                logAt(level, trunkSetter, random, cursor, tree, origin, 0, rise, 1);
            }
        }
        return List.of(new FoliagePlacer.FoliageAttachment(origin.above(treeHeight), 0, true));
    }
}

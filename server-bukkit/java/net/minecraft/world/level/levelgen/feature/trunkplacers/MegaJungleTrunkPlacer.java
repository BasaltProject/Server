// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public class MegaJungleTrunkPlacer extends GiantTrunkPlacer {
    public static final MapCodec<MegaJungleTrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.trunkplacers.MegaJungleTrunkPlacer.class, "CODEC");
    public MegaJungleTrunkPlacer(int baseHeight, int heightRandA, int heightRandB) {
        super(baseHeight, heightRandA, heightRandB);
    }
    protected TrunkPlacerType<?> type() {
        return TrunkPlacerType.MEGA_JUNGLE_TRUNK_PLACER;
    }
    public List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree) {
        List<FoliagePlacer.FoliageAttachment> found = new java.util.ArrayList<>(super.placeTrunk(level, trunkSetter, random, treeHeight, origin, tree));
        int cursorHeight = treeHeight - 2 - random.nextInt(4);
        while (cursorHeight > treeHeight / 2) {
            float angle = random.nextFloat() * ((float) Math.PI * 2F);
            int offX = 0;
            int offZ = 0;
            for (int reach = 0; reach < 5; reach++) {
                offX = (int) (1.5F + Mth.cos(angle) * (float) reach);
                offZ = (int) (1.5F + Mth.sin(angle) * (float) reach);
                placeLog(level, trunkSetter, random, origin.offset(offX, cursorHeight - 3 + reach / 2, offZ), tree);
            }
            found.add(new FoliagePlacer.FoliageAttachment(origin.offset(offX, cursorHeight, offZ), -2, false));
            cursorHeight -= 2 + random.nextInt(4);
        }
        return found;
    }
}

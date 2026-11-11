// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import com.mojang.datafixers.Products;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
public abstract class TrunkPlacer {
    public static final Codec<TrunkPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.trunkplacers.TrunkPlacer.class, "CODEC");
    public static final int MAX_HEIGHT = 80;
    protected final int baseHeight;
    protected final int heightRandA;
    protected final int heightRandB;
    protected static <P extends TrunkPlacer> Products.P3<RecordCodecBuilder.Mu<P>, Integer, Integer, Integer> trunkPlacerParts(RecordCodecBuilder.Instance<P> instance) {
        return instance.group(Codec.intRange(0, 32).fieldOf("base_height").forGetter(p -> p.baseHeight),
            Codec.intRange(0, 24).fieldOf("height_rand_a").forGetter(p -> p.heightRandA),
            Codec.intRange(0, 24).fieldOf("height_rand_b").forGetter(p -> p.heightRandB));
    }
    public TrunkPlacer(int baseHeight, int heightRandA, int heightRandB) {
        this.baseHeight = baseHeight;
        this.heightRandA = heightRandA;
        this.heightRandB = heightRandB;
    }
    protected abstract TrunkPlacerType<?> type();
    public abstract List<FoliagePlacer.FoliageAttachment> placeTrunk(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, int treeHeight, BlockPos origin, TreeFeature tree);
    public int getBaseHeight() {
        return baseHeight;
    }
    public int getTreeHeight(RandomSource random) {
        return baseHeight + random.nextInt(heightRandA + 1) + random.nextInt(heightRandB + 1);
    }
    protected static void placeBelowTrunkBlock(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, BlockPos pos, TreeFeature tree) {
        BlockState under = ((BlockStateProvider) tree.belowTrunkProvider().value()).getOptionalState(level, random, pos);
        if (under != null) {
            trunkSetter.accept(pos, under);
        }
    }
    protected boolean placeLog(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, BlockPos pos, TreeFeature tree) {
        return placeLog(level, trunkSetter, random, pos, tree, Function.identity());
    }
    protected boolean placeLog(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, BlockPos pos, TreeFeature tree, Function<BlockState, BlockState> stateModifier) {
        if (!validTreePos(level, pos)) {
            return false;
        }
        BlockState log = ((BlockStateProvider) tree.trunkProvider().value()).getState(level, random, pos);
        trunkSetter.accept(pos, stateModifier.apply(log));
        return true;
    }
    protected void placeLogIfFree(WorldGenLevel level, BiConsumer<BlockPos, BlockState> trunkSetter, RandomSource random, BlockPos.MutableBlockPos pos, TreeFeature tree) {
        if (isFree(level, pos)) {
            placeLog(level, trunkSetter, random, pos, tree);
        }
    }
    protected boolean validTreePos(WorldGenLevel level, BlockPos pos) {
        return TreeFeature.validTreePos(level, pos);
    }
    public boolean isFree(WorldGenLevel level, BlockPos pos) {
        return validTreePos(level, pos) || level.isStateAtPosition(pos, s -> s.is(net.minecraft.tags.BlockTags.LOGS));
    }
}

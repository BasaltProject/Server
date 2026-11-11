// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record BlockColumnFeature(List<Layer> layers, Direction direction, BlockPredicate allowedPlacement, boolean prioritizeTip) implements Feature {
    private static void trim(int[] heights, int total, int keep, boolean tipFirst) {
        int excess = total - keep;
        int step = tipFirst ? 1 : -1;
        int from = tipFirst ? 0 : heights.length - 1;
        int stop = tipFirst ? heights.length : -1;
        for (int i = from; i != stop && excess > 0; i += step) {
            int cut = Math.min(heights[i], excess);
            excess -= cut;
            heights[i] -= cut;
        }
    }
    public static final MapCodec<BlockColumnFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.BlockColumnFeature.class, "CODEC");
    public static Layer layer(IntProvider height, Holder<BlockStateProvider> state) {
        return new Layer(height, state);
    }
    public static Layer layer(IntProvider height, BlockStateProvider state) {
        return layer(height, Holder.direct(state));
    }
    public static BlockColumnFeature simple(IntProvider height, BlockStateProvider state) {
        return new BlockColumnFeature(List.of(layer(height, Holder.direct(state))), Direction.UP, BlockPredicate.ONLY_IN_AIR_PREDICATE, false);
    }
    public MapCodec<BlockColumnFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        int[] heights = new int[layers.size()];
        int total = 0;
        for (int i = 0; i < heights.length; i++) {
            heights[i] = layers.get(i).height().sample(random);
            total += heights[i];
        }
        if (total == 0) {
            return false;
        }
        BlockPos.MutableBlockPos cursor = origin.mutable();
        BlockPos.MutableBlockPos probe = cursor.mutable().move(direction);
        for (int step = 0; step < total; step++) {
            if (!allowedPlacement.test(level, probe)) {
                trim(heights, total, step, prioritizeTip);
                break;
            }
            probe.move(direction);
        }
        for (int i = 0; i < heights.length; i++) {
            for (int step = 0; step < heights[i]; step++) {
                level.setBlock(cursor, layers.get(i).state().value().getState(level, random, cursor), 2);
                cursor.move(direction);
            }
        }
        return true;
    }
    public static record Layer(IntProvider height, Holder<BlockStateProvider> state) {
        public static final Codec<Layer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.BlockColumnFeature.Layer.class, "CODEC");
    }
}

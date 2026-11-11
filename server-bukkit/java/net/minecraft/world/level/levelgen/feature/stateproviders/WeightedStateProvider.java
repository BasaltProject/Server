// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
public record WeightedStateProvider(WeightedList<BlockState> weightedList) implements BlockStateProvider {
    public static final MapCodec<WeightedStateProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.WeightedStateProvider.class, "CODEC");
    public WeightedStateProvider(WeightedList.Builder<BlockState> weightedList) {
        this(weightedList.build());
    }
    public MapCodec<WeightedStateProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return weightedList.getRandomOrThrow(random);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
public record SimpleStateProvider(BlockState state) implements BlockStateProvider {
    public static final MapCodec<SimpleStateProvider> CODEC = BlockState.CODEC.fieldOf("state").xmap(SimpleStateProvider::new, SimpleStateProvider::state);
    public MapCodec<SimpleStateProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return state;
    }
}

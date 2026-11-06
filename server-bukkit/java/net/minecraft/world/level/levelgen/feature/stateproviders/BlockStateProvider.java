// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public interface BlockStateProvider {
    Codec<BlockStateProvider> TYPED_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider.class, "TYPED_CODEC");
    Codec<Either<BlockState, BlockStateProvider>> STATE_OR_PROVIDER_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider.class, "STATE_OR_PROVIDER_CODEC");
    Codec<BlockStateProvider> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider.class, "DIRECT_CODEC");
    Codec<Holder<BlockStateProvider>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider.class, "CODEC");
    static SimpleStateProvider of(BlockState state) {
        return new SimpleStateProvider(state);
    }
    static SimpleStateProvider of(Block block) {
        return new SimpleStateProvider(block.defaultBlockState());
    }
    static Holder<BlockStateProvider> holderOf(BlockState state) {
        return Holder.direct(of(state));
    }
    static Holder<BlockStateProvider> holderOf(Block block) {
        return Holder.direct(of(block));
    }
    MapCodec<? extends BlockStateProvider> codec();
    BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos);
    default BlockState getOptionalState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return getState(level, random, pos);
    }
}

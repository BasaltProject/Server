// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public record CopyPropertiesProvider(Holder<BlockStateProvider> source) implements BlockStateProvider {
    public static final MapCodec<CopyPropertiesProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.CopyPropertiesProvider.class, "CODEC");
    public CopyPropertiesProvider(Block block) {
        this(BlockStateProvider.holderOf(block));
    }
    public MapCodec<CopyPropertiesProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return source.value().getState(level, random, pos).withPropertiesOf(level.getBlockState(pos));
    }
}

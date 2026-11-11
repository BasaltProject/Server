// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import net.minecraft.core.Holder;
import java.util.Optional;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public record RandomBlockProvider(HolderSet<Block> blocks) implements BlockStateProvider {
    private Optional<BlockState> pick(RandomSource random) {
        return blocks.getRandomElement(random).map(Holder::value).map(Block::defaultBlockState);
    }
    public static final MapCodec<RandomBlockProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.RandomBlockProvider.class, "CODEC");
    public MapCodec<RandomBlockProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return pick(random).orElseGet(() -> level.getBlockState(pos));
    }
    public BlockState getOptionalState(LevelAccessor level, RandomSource random, BlockPos pos) {
        return pick(random).orElse(null);
    }
}

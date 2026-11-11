// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
public record RotatedBlockProvider(Holder<BlockStateProvider> state, Optional<Direction> direction) implements BlockStateProvider {
    public static final MapCodec<RotatedBlockProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.RotatedBlockProvider.class, "CODEC");
    public RotatedBlockProvider(BlockStateProvider state) {
        this(Holder.direct(state), Optional.empty());
    }
    public MapCodec<RotatedBlockProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        Direction facing = direction.orElseGet(() -> Direction.getRandom(random));
        BlockState made = state.value().getState(level, random, pos).trySetValue(BlockStateProperties.AXIS, facing.getAxis()).trySetValue(BlockStateProperties.FACING, facing);
        return facing.getAxis().isHorizontal() ? made.trySetValue(BlockStateProperties.HORIZONTAL_FACING, facing) : made;
    }
}
